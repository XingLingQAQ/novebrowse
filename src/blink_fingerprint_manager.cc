#include "novebrowse/blink_fingerprint_manager.h"

#include <algorithm>
#include <random>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "novebrowse/fingerprint_manager.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/script/classic_script.h"

namespace novebrowse {

const char BlinkFingerprintManager::kSupplementName[] = "BlinkFingerprintManager";

// static
BlinkFingerprintManager* BlinkFingerprintManager::FromFrame(blink::LocalFrame* frame) {
  if (!frame) {
    return nullptr;
  }
  
  BlinkFingerprintManager* manager = Supplement<blink::LocalFrame>::From<BlinkFingerprintManager>(*frame);
  if (!manager) {
    manager = Create(frame);
  }
  
  return manager;
}

// static
BlinkFingerprintManager* BlinkFingerprintManager::Create(blink::LocalFrame* frame) {
  if (!frame) {
    return nullptr;
  }
  
  BlinkFingerprintManager* manager = MakeGarbageCollected<BlinkFingerprintManager>(*frame);
  Supplement<blink::LocalFrame>::ProvideTo(*frame, manager);
  return manager;
}

BlinkFingerprintManager::BlinkFingerprintManager(blink::LocalFrame& frame) 
    : Supplement<blink::LocalFrame>(frame), frame_(&frame) {
  InitializeDefaultConfig();
}

void BlinkFingerprintManager::UpdateConfig(const FingerprintConfig& config) {
  if (!ValidateConfig(config)) {
    LOG(ERROR) << "Invalid fingerprint configuration provided";
    return;
  }
  
  config_ = config;
  configured_ = true;
  ClearCache();
  
  LOG(INFO) << "Updated fingerprint configuration for frame";
}

WTF::String BlinkFingerprintManager::GetSpoofedUserAgent() const {
  if (!configured_ || !config_.navigator.enabled) {
    return WTF::String();
  }
  
  if (cached_user_agent_.IsEmpty()) {
    cached_user_agent_ = WTF::String::FromUTF8(config_.navigator.user_agent.c_str());
  }
  
  IncrementOperationCount("navigator_user_agent");
  return cached_user_agent_;
}

WTF::String BlinkFingerprintManager::GetSpoofedPlatform() const {
  if (!configured_ || !config_.navigator.enabled) {
    return WTF::String();
  }
  
  if (cached_platform_.IsEmpty()) {
    cached_platform_ = WTF::String::FromUTF8(config_.navigator.platform.c_str());
  }
  
  IncrementOperationCount("navigator_platform");
  return cached_platform_;
}

WTF::Vector<WTF::String> BlinkFingerprintManager::GetSpoofedLanguages() const {
  if (!configured_ || !config_.navigator.enabled) {
    return WTF::Vector<WTF::String>();
  }
  
  if (cached_languages_.IsEmpty()) {
    for (const auto& lang : config_.navigator.languages) {
      cached_languages_.push_back(WTF::String::FromUTF8(lang.c_str()));
    }
  }
  
  IncrementOperationCount("navigator_languages");
  return cached_languages_;
}

int BlinkFingerprintManager::GetSpoofedHardwareConcurrency() const {
  if (!configured_ || !config_.navigator.enabled) {
    return 0;
  }
  
  IncrementOperationCount("navigator_hardware_concurrency");
  return config_.navigator.hardware_concurrency;
}

uint64_t BlinkFingerprintManager::GetSpoofedDeviceMemory() const {
  if (!configured_ || !config_.navigator.enabled) {
    return 0;
  }
  
  IncrementOperationCount("navigator_device_memory");
  return config_.navigator.device_memory;
}

bool BlinkFingerprintManager::ShouldHideWebDriver() const {
  if (!configured_ || !config_.navigator.enabled) {
    return false;
  }
  
  IncrementOperationCount("navigator_webdriver");
  return config_.navigator.hide_webdriver;
}

int BlinkFingerprintManager::GetSpoofedScreenWidth() const {
  if (!configured_ || !config_.screen.enabled) {
    return 0;
  }
  
  IncrementOperationCount("screen_width");
  return config_.screen.width;
}

int BlinkFingerprintManager::GetSpoofedScreenHeight() const {
  if (!configured_ || !config_.screen.enabled) {
    return 0;
  }
  
  IncrementOperationCount("screen_height");
  return config_.screen.height;
}

int BlinkFingerprintManager::GetSpoofedScreenColorDepth() const {
  if (!configured_ || !config_.screen.enabled) {
    return 0;
  }
  
  IncrementOperationCount("screen_color_depth");
  return config_.screen.color_depth;
}

int BlinkFingerprintManager::GetSpoofedScreenPixelDepth() const {
  if (!configured_ || !config_.screen.enabled) {
    return 0;
  }
  
  IncrementOperationCount("screen_pixel_depth");
  return config_.screen.pixel_depth;
}

double BlinkFingerprintManager::GetSpoofedDevicePixelRatio() const {
  if (!configured_ || !config_.screen.enabled) {
    return 0.0;
  }
  
  IncrementOperationCount("screen_device_pixel_ratio");
  return config_.screen.device_pixel_ratio;
}

WTF::String BlinkFingerprintManager::GetSpoofedTimezone() const {
  if (!configured_ || !config_.timezone.enabled) {
    return WTF::String();
  }
  
  IncrementOperationCount("timezone");
  return WTF::String::FromUTF8(config_.timezone.timezone.c_str());
}

int BlinkFingerprintManager::GetSpoofedTimezoneOffset() const {
  if (!configured_ || !config_.timezone.enabled) {
    return 0;
  }
  
  IncrementOperationCount("timezone_offset");
  return config_.timezone.timezone_offset;
}

bool BlinkFingerprintManager::ShouldSpoofGeolocation() const {
  return configured_ && config_.geolocation.enabled && config_.geolocation.spoof_location;
}

double BlinkFingerprintManager::GetSpoofedLatitude() const {
  if (!ShouldSpoofGeolocation()) {
    return 0.0;
  }
  
  IncrementOperationCount("geolocation_latitude");
  return config_.geolocation.latitude;
}

double BlinkFingerprintManager::GetSpoofedLongitude() const {
  if (!ShouldSpoofGeolocation()) {
    return 0.0;
  }
  
  IncrementOperationCount("geolocation_longitude");
  return config_.geolocation.longitude;
}

double BlinkFingerprintManager::GetSpoofedAccuracy() const {
  if (!ShouldSpoofGeolocation()) {
    return 0.0;
  }
  
  IncrementOperationCount("geolocation_accuracy");
  return config_.geolocation.accuracy;
}

bool BlinkFingerprintManager::ShouldProtectCanvas() const {
  return configured_ && config_.canvas.enabled;
}

double BlinkFingerprintManager::GetCanvasNoiseLevel() const {
  if (!ShouldProtectCanvas()) {
    return 0.0;
  }
  
  return config_.canvas.noise_level;
}

bool BlinkFingerprintManager::ShouldSpoofTextMetrics() const {
  return configured_ && config_.canvas.enabled && config_.canvas.spoof_text_metrics;
}

bool BlinkFingerprintManager::ShouldProtectWebGL() const {
  return configured_ && config_.webgl.enabled;
}

WTF::String BlinkFingerprintManager::GetSpoofedWebGLVendor() const {
  if (!ShouldProtectWebGL()) {
    return WTF::String();
  }
  
  IncrementOperationCount("webgl_vendor");
  return WTF::String::FromUTF8(config_.webgl.vendor.c_str());
}

WTF::String BlinkFingerprintManager::GetSpoofedWebGLRenderer() const {
  if (!ShouldProtectWebGL()) {
    return WTF::String();
  }
  
  IncrementOperationCount("webgl_renderer");
  return WTF::String::FromUTF8(config_.webgl.renderer.c_str());
}

WTF::String BlinkFingerprintManager::GetSpoofedWebGLVersion() const {
  if (!ShouldProtectWebGL()) {
    return WTF::String();
  }
  
  IncrementOperationCount("webgl_version");
  return WTF::String::FromUTF8(config_.webgl.version.c_str());
}

WTF::Vector<WTF::String> BlinkFingerprintManager::GetSpoofedWebGLExtensions() const {
  if (!ShouldProtectWebGL()) {
    return WTF::Vector<WTF::String>();
  }
  
  if (cached_webgl_extensions_.IsEmpty()) {
    for (const auto& ext : config_.webgl.extensions) {
      cached_webgl_extensions_.push_back(WTF::String::FromUTF8(ext.c_str()));
    }
  }
  
  IncrementOperationCount("webgl_extensions");
  return cached_webgl_extensions_;
}

bool BlinkFingerprintManager::ShouldProtectAudio() const {
  return configured_ && config_.audio.enabled;
}

double BlinkFingerprintManager::GetAudioNoiseLevel() const {
  if (!ShouldProtectAudio()) {
    return 0.0;
  }
  
  return config_.audio.noise_level;
}

int BlinkFingerprintManager::GetSpoofedSampleRate() const {
  if (!ShouldProtectAudio()) {
    return 0;
  }
  
  IncrementOperationCount("audio_sample_rate");
  return config_.audio.sample_rate;
}

bool BlinkFingerprintManager::ShouldProtectFonts() const {
  return configured_ && config_.font.enabled;
}

WTF::Vector<WTF::String> BlinkFingerprintManager::GetSpoofedAvailableFonts() const {
  if (!ShouldProtectFonts()) {
    return WTF::Vector<WTF::String>();
  }
  
  if (cached_fonts_.IsEmpty()) {
    for (const auto& font : config_.font.available_fonts) {
      cached_fonts_.push_back(WTF::String::FromUTF8(font.c_str()));
    }
  }
  
  IncrementOperationCount("font_enumeration");
  return cached_fonts_;
}

bool BlinkFingerprintManager::ShouldSpoofFontMetrics() const {
  return configured_ && config_.font.enabled && config_.font.spoof_metrics;
}

bool BlinkFingerprintManager::ShouldProtectWebRTC() const {
  return configured_ && config_.webrtc.enabled;
}

bool BlinkFingerprintManager::ShouldMaskLocalIPs() const {
  return ShouldProtectWebRTC() && config_.webrtc.mask_local_ips;
}

WTF::String BlinkFingerprintManager::GetFakePublicIP() const {
  if (!ShouldProtectWebRTC()) {
    return WTF::String();
  }
  
  IncrementOperationCount("webrtc_fake_ip");
  return WTF::String::FromUTF8(config_.webrtc.fake_public_ip.c_str());
}

bool BlinkFingerprintManager::ShouldHideAutomationFlags() const {
  return configured_ && config_.anti_detection.enabled && 
         config_.anti_detection.automation.hide_headless_flags;
}

bool BlinkFingerprintManager::ShouldSpoofChromeRuntime() const {
  return configured_ && config_.anti_detection.enabled && 
         config_.anti_detection.webdriver.spoof_chrome_runtime;
}

bool BlinkFingerprintManager::ShouldBlockDetectionScripts() const {
  return configured_ && config_.anti_detection.enabled && 
         config_.anti_detection.js_injection.block_detection_scripts;
}

WTF::Vector<WTF::String> BlinkFingerprintManager::GetBlockedScriptPatterns() const {
  if (!ShouldBlockDetectionScripts()) {
    return WTF::Vector<WTF::String>();
  }
  
  WTF::Vector<WTF::String> patterns;
  for (const auto& pattern : config_.anti_detection.js_injection.blocked_script_patterns) {
    patterns.push_back(WTF::String::FromUTF8(pattern.c_str()));
  }
  
  return patterns;
}

void BlinkFingerprintManager::IncrementOperationCount(const WTF::String& operation) {
  std::string op_str = operation.Utf8();
  operation_counts_[op_str]++;
}

int BlinkFingerprintManager::GetOperationCount(const WTF::String& operation) const {
  std::string op_str = operation.Utf8();
  auto it = operation_counts_.find(op_str);
  return it != operation_counts_.end() ? it->second : 0;
}

void BlinkFingerprintManager::Trace(blink::Visitor* visitor) const {
  visitor->Trace(frame_);
  Supplement<blink::LocalFrame>::Trace(visitor);
}

void BlinkFingerprintManager::InitializeDefaultConfig() {
  // Initialize with basic default configuration
  config_.enabled = true;
  config_.profile_name = "default";
  config_.device_profile = "windows_desktop";
  config_.behavior_pattern = "normal_user";
  
  // Basic navigator config
  config_.navigator.enabled = true;
  config_.navigator.user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
  config_.navigator.platform = "Win32";
  config_.navigator.languages = {"en-US", "en"};
  config_.navigator.hardware_concurrency = 8;
  config_.navigator.device_memory = 8;
  config_.navigator.hide_webdriver = true;
  
  // Basic canvas config
  config_.canvas.enabled = true;
  config_.canvas.add_noise = true;
  config_.canvas.noise_level = 0.1;
  config_.canvas.spoof_text_metrics = true;
  
  // Basic WebGL config
  config_.webgl.enabled = true;
  config_.webgl.vendor = "Google Inc. (Intel)";
  config_.webgl.renderer = "ANGLE (Intel, Intel(R) UHD Graphics 620 Direct3D11 vs_5_0 ps_5_0, D3D11)";
  config_.webgl.version = "OpenGL ES 2.0 (ANGLE 2.1.0.0)";
  
  // Basic screen config
  config_.screen.enabled = true;
  config_.screen.width = 1920;
  config_.screen.height = 1080;
  config_.screen.color_depth = 24;
  config_.screen.pixel_depth = 24;
  config_.screen.device_pixel_ratio = 1.0;
  
  // Basic anti-detection config
  config_.anti_detection.enabled = true;
  config_.anti_detection.webdriver.hide_webdriver_property = true;
  config_.anti_detection.webdriver.hide_automation_flags = true;
  config_.anti_detection.automation.hide_headless_flags = true;
  
  configured_ = false; // Will be set to true when real config is loaded
}

bool BlinkFingerprintManager::ValidateConfig(const FingerprintConfig& config) const {
  // Basic validation
  if (config.profile_name.empty()) {
    return false;
  }
  
  if (config.navigator.enabled && config.navigator.user_agent.empty()) {
    return false;
  }
  
  if (config.screen.enabled && (config.screen.width <= 0 || config.screen.height <= 0)) {
    return false;
  }
  
  return true;
}

uint32_t BlinkFingerprintManager::GenerateSeed() const {
  if (!frame_) {
    return 12345;
  }
  
  std::hash<std::string> hasher;
  std::string frame_id = base::StringPrintf("frame_%p", frame_.Get());
  return static_cast<uint32_t>(hasher(frame_id));
}

WTF::String BlinkFingerprintManager::GenerateConsistentValue(
    const WTF::String& base, uint32_t seed) const {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<int> dis(0, 255);
  
  std::string base_str = base.Utf8();
  std::string result = base_str;
  
  // Add small random variation
  if (!result.empty()) {
    result[result.length() - 1] = static_cast<char>(dis(gen) % 26 + 'a');
  }
  
  return WTF::String::FromUTF8(result.c_str());
}

void BlinkFingerprintManager::ClearCache() const {
  cached_user_agent_ = WTF::String();
  cached_platform_ = WTF::String();
  cached_languages_.clear();
  cached_webgl_extensions_.clear();
  cached_fonts_.clear();
  cache_valid_ = false;
}

// JSInjectionManager implementation
// static
void JSInjectionManager::InjectAntiDetectionScripts(blink::LocalFrame* frame) {
  if (!frame) {
    return;
  }
  
  WTF::String script = GenerateAntiDetectionScript();
  InjectCustomScript(frame, script);
}

// static
void JSInjectionManager::InjectCanvasProtection(blink::LocalFrame* frame, 
                                               const CanvasConfig& config) {
  if (!frame || !config.enabled) {
    return;
  }
  
  WTF::String script = GenerateCanvasProtectionScript(config);
  InjectCustomScript(frame, script);
}

// static
void JSInjectionManager::InjectWebGLProtection(blink::LocalFrame* frame, 
                                              const WebGLConfig& config) {
  if (!frame || !config.enabled) {
    return;
  }
  
  WTF::String script = GenerateWebGLProtectionScript(config);
  InjectCustomScript(frame, script);
}

// static
void JSInjectionManager::InjectNavigatorSpoofing(blink::LocalFrame* frame, 
                                                 const NavigatorConfig& config) {
  if (!frame || !config.enabled) {
    return;
  }
  
  WTF::String script = GenerateNavigatorSpoofingScript(config);
  InjectCustomScript(frame, script);
}

// static
void JSInjectionManager::InjectWebRTCProtection(blink::LocalFrame* frame, 
                                               const WebRTCConfig& config) {
  if (!frame || !config.enabled) {
    return;
  }
  
  WTF::String script = GenerateWebRTCProtectionScript(config);
  InjectCustomScript(frame, script);
}

// static
void JSInjectionManager::InjectCustomScript(blink::LocalFrame* frame, 
                                           const WTF::String& script) {
  if (!frame || script.IsEmpty()) {
    return;
  }
  
  blink::LocalDOMWindow* window = frame->DomWindow();
  if (!window) {
    return;
  }
  
  blink::ClassicScript* classic_script = blink::ClassicScript::Create(
      script, blink::ScriptSourceLocationType::kInternal);
  
  if (classic_script) {
    classic_script->RunScript(window);
  }
}

// static
WTF::String JSInjectionManager::GenerateAntiDetectionScript() {
  return WTF::String(kAntiDetectionTemplate);
}

// static
WTF::String JSInjectionManager::GenerateCanvasProtectionScript(const CanvasConfig& config) {
  WTF::String script = WTF::String(kCanvasProtectionTemplate);
  // Replace placeholders with actual config values
  script = script.Replace("{{NOISE_LEVEL}}", WTF::String::Number(config.noise_level));
  script = script.Replace("{{SPOOF_TEXT_METRICS}}", config.spoof_text_metrics ? "true" : "false");
  return script;
}

// static
WTF::String JSInjectionManager::GenerateWebGLProtectionScript(const WebGLConfig& config) {
  WTF::String script = WTF::String(kWebGLProtectionTemplate);
  script = script.Replace("{{VENDOR}}", WTF::String::FromUTF8(config.vendor.c_str()));
  script = script.Replace("{{RENDERER}}", WTF::String::FromUTF8(config.renderer.c_str()));
  return script;
}

// static
WTF::String JSInjectionManager::GenerateNavigatorSpoofingScript(const NavigatorConfig& config) {
  WTF::String script = WTF::String(kNavigatorSpoofingTemplate);
  script = script.Replace("{{USER_AGENT}}", WTF::String::FromUTF8(config.user_agent.c_str()));
  script = script.Replace("{{PLATFORM}}", WTF::String::FromUTF8(config.platform.c_str()));
  return script;
}

// static
WTF::String JSInjectionManager::GenerateWebRTCProtectionScript(const WebRTCConfig& config) {
  WTF::String script = WTF::String(kWebRTCProtectionTemplate);
  script = script.Replace("{{MASK_IPS}}", config.mask_local_ips ? "true" : "false");
  return script;
}

// Script templates
const char JSInjectionManager::kAntiDetectionTemplate[] = R"(
(function() {
  'use strict';
  
  // Hide webdriver property
  Object.defineProperty(navigator, 'webdriver', {
    get: () => undefined,
    configurable: true
  });
  
  // Remove automation indicators
  delete window.cdc_adoQpoasnfa76pfcZLmcfl_Array;
  delete window.cdc_adoQpoasnfa76pfcZLmcfl_Promise;
  delete window.cdc_adoQpoasnfa76pfcZLmcfl_Symbol;
  delete window.$chrome_asyncScriptInfo;
  delete window.__webdriver_evaluate;
  delete window.__selenium_evaluate;
  delete window.__webdriver_script_function;
  delete window.__webdriver_script_func;
  delete window.__webdriver_script_fn;
  delete window.__fxdriver_evaluate;
  delete window.__driver_unwrapped;
  delete window.webdriver_id;
  
  // Spoof chrome runtime
  if (window.chrome && window.chrome.runtime) {
    Object.defineProperty(window.chrome.runtime, 'onConnect', {
      value: undefined,
      writable: false
    });
  }
})();
)";

const char JSInjectionManager::kCanvasProtectionTemplate[] = R"(
(function() {
  'use strict';
  
  const originalGetImageData = CanvasRenderingContext2D.prototype.getImageData;
  const originalToDataURL = HTMLCanvasElement.prototype.toDataURL;
  const originalMeasureText = CanvasRenderingContext2D.prototype.measureText;
  
  // Add noise to getImageData
  CanvasRenderingContext2D.prototype.getImageData = function(...args) {
    const imageData = originalGetImageData.apply(this, args);
    const data = imageData.data;
    const noiseLevel = {{NOISE_LEVEL}};
    
    for (let i = 0; i < data.length; i += 4) {
      const noise = (Math.random() - 0.5) * noiseLevel * 255;
      data[i] = Math.max(0, Math.min(255, data[i] + noise));
      data[i + 1] = Math.max(0, Math.min(255, data[i + 1] + noise));
      data[i + 2] = Math.max(0, Math.min(255, data[i + 2] + noise));
    }
    
    return imageData;
  };
  
  // Add noise to toDataURL
  HTMLCanvasElement.prototype.toDataURL = function(...args) {
    const context = this.getContext('2d');
    if (context) {
      const imageData = context.getImageData(0, 0, this.width, this.height);
      context.putImageData(imageData, 0, 0);
    }
    return originalToDataURL.apply(this, args);
  };
  
  // Spoof text metrics
  if ({{SPOOF_TEXT_METRICS}}) {
    CanvasRenderingContext2D.prototype.measureText = function(...args) {
      const metrics = originalMeasureText.apply(this, args);
      const offset = (Math.random() - 0.5) * 0.1;
      metrics.width += offset;
      return metrics;
    };
  }
})();
)";

const char JSInjectionManager::kWebGLProtectionTemplate[] = R"(
(function() {
  'use strict';
  
  const originalGetParameter = WebGLRenderingContext.prototype.getParameter;
  const originalGetExtension = WebGLRenderingContext.prototype.getExtension;
  
  WebGLRenderingContext.prototype.getParameter = function(parameter) {
    switch (parameter) {
      case this.VENDOR:
        return '{{VENDOR}}';
      case this.RENDERER:
        return '{{RENDERER}}';
      default:
        return originalGetParameter.apply(this, arguments);
    }
  };
  
  if (WebGL2RenderingContext) {
    WebGL2RenderingContext.prototype.getParameter = WebGLRenderingContext.prototype.getParameter;
  }
})();
)";

const char JSInjectionManager::kNavigatorSpoofingTemplate[] = R"(
(function() {
  'use strict';
  
  Object.defineProperty(navigator, 'userAgent', {
    get: () => '{{USER_AGENT}}',
    configurable: true
  });
  
  Object.defineProperty(navigator, 'platform', {
    get: () => '{{PLATFORM}}',
    configurable: true
  });
})();
)";

const char JSInjectionManager::kWebRTCProtectionTemplate[] = R"(
(function() {
  'use strict';
  
  if ({{MASK_IPS}}) {
    const originalCreateOffer = RTCPeerConnection.prototype.createOffer;
    const originalCreateAnswer = RTCPeerConnection.prototype.createAnswer;
    
    RTCPeerConnection.prototype.createOffer = function(...args) {
      return originalCreateOffer.apply(this, args).then(offer => {
        offer.sdp = offer.sdp.replace(/([0-9]{1,3}\.){3}[0-9]{1,3}/g, '127.0.0.1');
        return offer;
      });
    };
    
    RTCPeerConnection.prototype.createAnswer = function(...args) {
      return originalCreateAnswer.apply(this, args).then(answer => {
        answer.sdp = answer.sdp.replace(/([0-9]{1,3}\.){3}[0-9]{1,3}/g, '127.0.0.1');
        return answer;
      });
    };
  }
})();
)";

}  // namespace novebrowse
