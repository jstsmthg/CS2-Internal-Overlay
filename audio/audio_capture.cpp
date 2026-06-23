#include "pch.h"
#include "audio/audio_capture.h"
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propidl.h>

namespace {

static std::atomic<bool> g_running{false};
static std::thread g_thread;
static audio_capture::ChunkCallback g_callback;
static std::mutex g_callbackMutex;
static char g_status[256] = "Audio capture stopped";

static constexpr int kWhisperSampleRate = 16000;
static constexpr size_t kChunkSamples = kWhisperSampleRate * 4;

class ActivationHandler : public IActivateAudioInterfaceCompletionHandler {
public:
  ActivationHandler() : m_event(CreateEventA(nullptr, FALSE, FALSE, nullptr)) {}

  ~ActivationHandler() {
    if (m_client)
      m_client->Release();
    if (m_event)
      CloseHandle(m_event);
  }

  ULONG STDMETHODCALLTYPE AddRef() override {
    return (ULONG)InterlockedIncrement(&m_refCount);
  }

  ULONG STDMETHODCALLTYPE Release() override {
    LONG refs = InterlockedDecrement(&m_refCount);
    if (refs == 0)
      delete this;
    return (ULONG)refs;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override {
    if (!object)
      return E_POINTER;
    if (iid == __uuidof(IUnknown) ||
        iid == __uuidof(IActivateAudioInterfaceCompletionHandler)) {
      *object = static_cast<IActivateAudioInterfaceCompletionHandler *>(this);
      AddRef();
      return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE
  ActivateCompleted(IActivateAudioInterfaceAsyncOperation *operation) override {
    HRESULT activateResult = E_FAIL;
    IUnknown *unknown = nullptr;
    HRESULT hr = operation->GetActivateResult(&activateResult, &unknown);
    if (SUCCEEDED(hr))
      hr = activateResult;
    if (SUCCEEDED(hr) && unknown) {
      hr = unknown->QueryInterface(__uuidof(IAudioClient), (void **)&m_client);
    }
    if (unknown)
      unknown->Release();
    m_hr = hr;
    if (m_event)
      SetEvent(m_event);
    return S_OK;
  }

  HRESULT WaitForClient(IAudioClient **client) {
    if (!client)
      return E_POINTER;
    *client = nullptr;
    if (!m_event)
      return HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE);
    DWORD wait = WaitForSingleObject(m_event, 5000);
    if (wait != WAIT_OBJECT_0)
      return HRESULT_FROM_WIN32(wait == WAIT_TIMEOUT ? ERROR_TIMEOUT
                                                     : GetLastError());
    if (FAILED(m_hr) || !m_client)
      return FAILED(m_hr) ? m_hr : E_FAIL;
    m_client->AddRef();
    *client = m_client;
    return S_OK;
  }

private:
  LONG m_refCount = 1;
  HANDLE m_event = nullptr;
  HRESULT m_hr = E_FAIL;
  IAudioClient *m_client = nullptr;
};

static void SetStatus(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_status, sizeof(g_status), fmt, args);
  va_end(args);
}

template <typename T> static void SafeRelease(T **ptr) {
  if (*ptr) {
    (*ptr)->Release();
    *ptr = nullptr;
  }
}

static float ReadSample(const BYTE *data, WORD bitsPerSample, bool isFloat) {
  if (isFloat && bitsPerSample == 32)
    return *(const float *)data;
  if (bitsPerSample == 16)
    return (float)(*(const int16_t *)data) / 32768.0f;
  if (bitsPerSample == 32)
    return (float)(*(const int32_t *)data) / 2147483648.0f;
  if (bitsPerSample == 24) {
    int32_t value = (data[0] | (data[1] << 8) | (data[2] << 16));
    if (value & 0x800000)
      value |= 0xFF000000;
    return (float)value / 8388608.0f;
  }
  return 0.0f;
}

static void DispatchChunk(const std::vector<float> &chunk) {
  audio_capture::ChunkCallback callback;
  {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    callback = g_callback;
  }
  if (callback)
    callback(chunk);
}

static HRESULT ActivateProcessLoopback(IAudioClient **audioClient) {
  if (!audioClient)
    return E_POINTER;
  *audioClient = nullptr;

  AUDIOCLIENT_ACTIVATION_PARAMS params = {};
  params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
  params.ProcessLoopbackParams.ProcessLoopbackMode =
      PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
  params.ProcessLoopbackParams.TargetProcessId = GetCurrentProcessId();

  PROPVARIANT activationParams;
  PropVariantInit(&activationParams);
  activationParams.vt = VT_BLOB;
  activationParams.blob.cbSize = sizeof(params);
  activationParams.blob.pBlobData = reinterpret_cast<BYTE *>(&params);

  ActivationHandler *handler = new ActivationHandler();
  IActivateAudioInterfaceAsyncOperation *operation = nullptr;
  HRESULT hr = ActivateAudioInterfaceAsync(
      VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
      &activationParams, handler, &operation);
  if (SUCCEEDED(hr))
    hr = handler->WaitForClient(audioClient);
  if (operation)
    operation->Release();
  handler->Release();
  return hr;
}

static WAVEFORMATEX ProcessLoopbackFormat() {
  WAVEFORMATEX format = {};
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = 2;
  format.nSamplesPerSec = 44100;
  format.wBitsPerSample = 16;
  format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
  format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
  return format;
}

static void CaptureThread() {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool coInit = SUCCEEDED(hr);

  IMMDeviceEnumerator *enumerator = nullptr;
  IMMDevice *device = nullptr;
  IAudioClient *audioClient = nullptr;
  IAudioCaptureClient *captureClient = nullptr;
  WAVEFORMATEX *mixFormat = nullptr;
  WAVEFORMATEX processFormat = {};
  bool mixFormatAllocated = false;
  bool usingProcessLoopback = false;
  REFERENCE_TIME bufferDuration = 10000000;

  hr = ActivateProcessLoopback(&audioClient);
  if (SUCCEEDED(hr) && audioClient) {
    processFormat = ProcessLoopbackFormat();
    mixFormat = &processFormat;
    usingProcessLoopback = true;
    SetStatus("Process audio loopback activated");
  } else {
    SafeRelease(&audioClient);

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void **)&enumerator);
    if (FAILED(hr)) {
      SetStatus("MMDeviceEnumerator failed");
      goto done;
    }

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) {
      SetStatus("Default render device not found");
      goto done;
    }

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          (void **)&audioClient);
    if (FAILED(hr)) {
      SetStatus("IAudioClient activation failed");
      goto done;
    }

    hr = audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr) || !mixFormat) {
      SetStatus("GetMixFormat failed");
      goto done;
    }
    mixFormatAllocated = true;
    SetStatus("System audio loopback activated");
  }

  hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                               AUDCLNT_STREAMFLAGS_LOOPBACK, bufferDuration, 0,
                               mixFormat, nullptr);
  if (FAILED(hr)) {
    SetStatus(usingProcessLoopback ? "Process loopback initialize failed"
                                   : "WASAPI loopback initialize failed");
    goto done;
  }

  hr = audioClient->GetService(__uuidof(IAudioCaptureClient),
                               (void **)&captureClient);
  if (FAILED(hr)) {
    SetStatus("IAudioCaptureClient service failed");
    goto done;
  }

  hr = audioClient->Start();
  if (FAILED(hr)) {
    SetStatus("Audio capture start failed");
    goto done;
  }

  SetStatus(usingProcessLoopback ? "CS2 process audio capture running"
                                 : "System audio capture running");

  {
    std::vector<float> chunk;
    chunk.reserve(kChunkSamples);
    double resampleCursor = 0.0;
    double resampleStep = (double)mixFormat->nSamplesPerSec / kWhisperSampleRate;
    WORD channels = mixFormat->nChannels ? mixFormat->nChannels : 1;
    WORD bytesPerSample = mixFormat->wBitsPerSample / 8;
    bool isFloat = mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
      WAVEFORMATEXTENSIBLE *ext = (WAVEFORMATEXTENSIBLE *)mixFormat;
      isFloat = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    }

    while (g_running) {
      Sleep(10);

      UINT32 packetFrames = 0;
      hr = captureClient->GetNextPacketSize(&packetFrames);
      if (FAILED(hr))
        continue;

      while (packetFrames > 0) {
        BYTE *data = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        hr = captureClient->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
        if (FAILED(hr))
          break;

        for (UINT32 frame = 0; frame < frames; ++frame) {
          if (resampleCursor <= 0.0) {
            float mono = 0.0f;
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
              const BYTE *frameData =
                  data + frame * mixFormat->nBlockAlign;
              for (WORD ch = 0; ch < channels; ++ch) {
                mono += ReadSample(frameData + ch * bytesPerSample,
                                   mixFormat->wBitsPerSample, isFloat);
              }
              mono /= (float)channels;
            }
            chunk.push_back(mono);
            resampleCursor += resampleStep;
          }
          resampleCursor -= 1.0;

          if (chunk.size() >= kChunkSamples) {
            double sum = 0.0;
            for (float sample : chunk)
              sum += (double)sample * sample;
            float rms = (float)std::sqrt(sum / (double)chunk.size());
            if (rms >= hooks::transcriptVoiceRmsThreshold)
              DispatchChunk(chunk);
            chunk.clear();
          }
        }

        captureClient->ReleaseBuffer(frames);
        hr = captureClient->GetNextPacketSize(&packetFrames);
        if (FAILED(hr))
          break;
      }
    }

    audioClient->Stop();
  }

done:
  if (mixFormatAllocated && mixFormat)
    CoTaskMemFree(mixFormat);
  SafeRelease(&captureClient);
  SafeRelease(&audioClient);
  SafeRelease(&device);
  SafeRelease(&enumerator);
  if (coInit)
    CoUninitialize();
  if (!g_running)
    SetStatus("Audio capture stopped");
  g_running = false;
}

} // namespace

bool audio_capture::Start(ChunkCallback callback) {
  if (g_running)
    return true;

  {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_callback = callback;
  }

  g_running = true;
  g_thread = std::thread(CaptureThread);
  return true;
}

void audio_capture::Stop() {
  if (!g_running && !g_thread.joinable())
    return;
  g_running = false;
  if (g_thread.joinable())
    g_thread.join();
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  g_callback = nullptr;
}

bool audio_capture::IsRunning() {
  return g_running;
}

const char *audio_capture::Status() {
  return g_status;
}
