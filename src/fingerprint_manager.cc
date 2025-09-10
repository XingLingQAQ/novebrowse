#include "novebrowse/fingerprint_manager.h"

#include <algorithm>
#include <fstream>
#include <random>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace novebrowse {

// Static member initialization
bool FingerprintManager::enabled_ = true;

FingerprintManager::FingerprintManager() {
  InitializeDefaultConfig();
}

FingerprintManager::~FingerprintManager() = default;

// static
FingerprintManager* FingerprintManager::GetInstance() {
  return base::Singleton<FingerprintManager>::get();
}

// static
bool FingerprintManager::IsEnabled() {
  return enabled_;
}

// static
void FingerprintManager::SetEnabled(bool enabled) {
  enabled_ = enabled;
  if (enabled) {
    LOG(INFO) << "FingerprintManager: Protection enabled";
  } else {
    LOG(INFO) << "FingerprintManager: Protection disabled";
  }
}

bool FingerprintManager::LoadConfig(const std::string& config_path) {
  base::AutoLock auto_lock(lock_);
  
  std::string config_content;
  if (!base::ReadFileToString(base::FilePath::FromUTF8Unsafe(config_path), 
                              &config_content)) {
    LOG(ERROR) << "Failed to read fingerprint config file: " << config_path;
    return false;
  }
  
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(config_content);
  if (!parsed_json.has_value()) {
    LOG(ERROR) << "Failed to parse fingerprint config JSON: " 
               << parsed_json.error().message;
    return false;
  }
  
  if (!parsed_json->is_dict()) {
    LOG(ERROR) << "Fingerprint config must be a JSON object";
    return false;
  }
  
  default_config_ = FingerprintConfig::FromValue(*parsed_json);
  if (!default_config_.IsValid()) {
    LOG(ERROR) << "Invalid fingerprint configuration";
    auto errors = default_config_.GetValidationErrors();
    for (const auto& error : errors) {
      LOG(ERROR) << "Config validation error: " << error;
    }
    return false;
  }
  
  LOG(INFO) << "Loaded fingerprint configuration from: " << config_path;
  return true;
}

bool FingerprintManager::SaveConfig(const std::string& config_path) {
  base::AutoLock auto_lock(lock_);
  
  base::Value config_value = default_config_.ToValue();
  std::string config_json;
  if (!base::JSONWriter::WriteWithOptions(
          config_value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &config_json)) {
    LOG(ERROR) << "Failed to serialize fingerprint config to JSON";
    return false;
  }
  
  if (!base::WriteFile(base::FilePath::FromUTF8Unsafe(config_path), config_json)) {
    LOG(ERROR) << "Failed to write fingerprint config file: " << config_path;
    return false;
  }
  
  LOG(INFO) << "Saved fingerprint configuration to: " << config_path;
  return true;
}

void FingerprintManager::UpdateConfig(const FingerprintConfig& config) {
  base::AutoLock auto_lock(lock_);
  
  if (!config.IsValid()) {
    LOG(ERROR) << "Attempted to update with invalid fingerprint configuration";
    return;
  }
  
  default_config_ = config;
  default_config_.updated_at = base::Time::Now().ToJsTimeIgnoringNull();
  
  LOG(INFO) << "Updated fingerprint configuration";
}

FingerprintConfig FingerprintManager::GetConfigForFrame(
    content::RenderFrameHost* frame) {
  base::AutoLock auto_lock(lock_);
  
  if (!frame) {
    return default_config_;
  }
  
  std::string frame_id = GenerateFrameId(frame);
  auto it = frame_configs_.find(frame_id);
  if (it != frame_configs_.end()) {
    return it->second;
  }
  
  return default_config_;
}

void FingerprintManager::SetConfigForFrame(content::RenderFrameHost* frame,
                                          const FingerprintConfig& config) {
  base::AutoLock auto_lock(lock_);
  
  if (!frame) {
    LOG(ERROR) << "Cannot set config for null frame";
    return;
  }
  
  if (!config.IsValid()) {
    LOG(ERROR) << "Cannot set invalid config for frame";
    return;
  }
  
  std::string frame_id = GenerateFrameId(frame);
  frame_configs_[frame_id] = config;
  
  LOG(INFO) << "Set fingerprint config for frame: " << frame_id;
}

void FingerprintManager::RemoveFrameConfig(content::RenderFrameHost* frame) {
  base::AutoLock auto_lock(lock_);
  
  if (!frame) {
    return;
  }
  
  std::string frame_id = GenerateFrameId(frame);
  auto it = frame_configs_.find(frame_id);
  if (it != frame_configs_.end()) {
    frame_configs_.erase(it);
    LOG(INFO) << "Removed fingerprint config for frame: " << frame_id;
  }
}

void FingerprintManager::SetDefaultConfig(const FingerprintConfig& config) {
  base::AutoLock auto_lock(lock_);
  
  if (!config.IsValid()) {
    LOG(ERROR) << "Cannot set invalid default config";
    return;
  }
  
  default_config_ = config;
  LOG(INFO) << "Updated default fingerprint configuration";
}

bool FingerprintManager::LoadDeviceProfiles(const std::string& profiles_path) {
  base::AutoLock auto_lock(lock_);
  
  std::string profiles_content;
  if (!base::ReadFileToString(base::FilePath::FromUTF8Unsafe(profiles_path), 
                              &profiles_content)) {
    LOG(ERROR) << "Failed to read device profiles file: " << profiles_path;
    return false;
  }
  
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(profiles_content);
  if (!parsed_json.has_value()) {
    LOG(ERROR) << "Failed to parse device profiles JSON: " 
               << parsed_json.error().message;
    return false;
  }
  
  if (!parsed_json->is_dict()) {
    LOG(ERROR) << "Device profiles must be a JSON object";
    return false;
  }
  
  const base::Value::Dict& profiles_dict = parsed_json->GetDict();
  const base::Value::Dict* profiles = profiles_dict.FindDict("profiles");
  if (!profiles) {
    LOG(ERROR) << "Device profiles file missing 'profiles' section";
    return false;
  }
  
  device_profiles_.clear();
  for (const auto& [profile_name, profile_value] : *profiles) {
    if (!profile_value.is_dict()) {
      LOG(WARNING) << "Skipping invalid device profile: " << profile_name;
      continue;
    }
    
    DeviceProfile profile;
    profile.name = profile_name;
    
    const base::Value::Dict& profile_dict = profile_value.GetDict();
    const std::string* description = profile_dict.FindString("description");
    if (description) {
      profile.description = *description;
    }
    
    // Parse navigator config
    const base::Value::Dict* navigator = profile_dict.FindDict("navigator");
    if (navigator) {
      const std::string* user_agent = navigator->FindString("user_agent");
      if (user_agent) profile.navigator.user_agent = *user_agent;
      
      const std::string* platform = navigator->FindString("platform");
      if (platform) profile.navigator.platform = *platform;
      
      const base::Value::List* languages = navigator->FindList("languages");
      if (languages) {
        profile.navigator.languages.clear();
        for (const auto& lang : *languages) {
          if (lang.is_string()) {
            profile.navigator.languages.push_back(lang.GetString());
          }
        }
      }
      
      std::optional<int> hardware_concurrency = navigator->FindInt("hardware_concurrency");
      if (hardware_concurrency) {
        profile.navigator.hardware_concurrency = *hardware_concurrency;
      }
      
      std::optional<double> device_memory = navigator->FindDouble("device_memory");
      if (device_memory) {
        profile.navigator.device_memory = static_cast<uint64_t>(*device_memory);
      }
    }
    
    // Parse screen config
    const base::Value::Dict* screen = profile_dict.FindDict("screen");
    if (screen) {
      std::optional<int> width = screen->FindInt("width");
      if (width) profile.screen.width = *width;
      
      std::optional<int> height = screen->FindInt("height");
      if (height) profile.screen.height = *height;
      
      std::optional<int> color_depth = screen->FindInt("color_depth");
      if (color_depth) profile.screen.color_depth = *color_depth;
      
      std::optional<int> pixel_depth = screen->FindInt("pixel_depth");
      if (pixel_depth) profile.screen.pixel_depth = *pixel_depth;
      
      std::optional<double> device_pixel_ratio = screen->FindDouble("device_pixel_ratio");
      if (device_pixel_ratio) profile.screen.device_pixel_ratio = *device_pixel_ratio;
    }
    
    // Parse WebGL config
    const base::Value::Dict* webgl = profile_dict.FindDict("webgl");
    if (webgl) {
      const std::string* vendor = webgl->FindString("vendor");
      if (vendor) profile.webgl.vendor = *vendor;
      
      const std::string* renderer = webgl->FindString("renderer");
      if (renderer) profile.webgl.renderer = *renderer;
    }
    
    device_profiles_[profile_name] = std::move(profile);
  }
  
  LOG(INFO) << "Loaded " << device_profiles_.size() << " device profiles from: " 
            << profiles_path;
  return true;
}

std::vector<std::string> FingerprintManager::GetAvailableProfiles() const {
  base::AutoLock auto_lock(lock_);
  
  std::vector<std::string> profile_names;
  profile_names.reserve(device_profiles_.size());
  
  for (const auto& [name, profile] : device_profiles_) {
    profile_names.push_back(name);
  }
  
  return profile_names;
}

DeviceProfile FingerprintManager::GetDeviceProfile(const std::string& profile_name) const {
  base::AutoLock auto_lock(lock_);
  
  auto it = device_profiles_.find(profile_name);
  if (it != device_profiles_.end()) {
    return it->second;
  }
  
  LOG(WARNING) << "Device profile not found: " << profile_name;
  return DeviceProfile{};
}

bool FingerprintManager::LoadBehaviorPatterns(const std::string& patterns_path) {
  base::AutoLock auto_lock(lock_);
  
  std::string patterns_content;
  if (!base::ReadFileToString(base::FilePath::FromUTF8Unsafe(patterns_path), 
                              &patterns_content)) {
    LOG(ERROR) << "Failed to read behavior patterns file: " << patterns_path;
    return false;
  }
  
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(patterns_content);
  if (!parsed_json.has_value()) {
    LOG(ERROR) << "Failed to parse behavior patterns JSON: " 
               << parsed_json.error().message;
    return false;
  }
  
  if (!parsed_json->is_dict()) {
    LOG(ERROR) << "Behavior patterns must be a JSON object";
    return false;
  }
  
  const base::Value::Dict& patterns_dict = parsed_json->GetDict();
  const base::Value::Dict* patterns = patterns_dict.FindDict("patterns");
  if (!patterns) {
    LOG(ERROR) << "Behavior patterns file missing 'patterns' section";
    return false;
  }
  
  behavior_patterns_.clear();
  for (const auto& [pattern_name, pattern_value] : *patterns) {
    if (!pattern_value.is_dict()) {
      LOG(WARNING) << "Skipping invalid behavior pattern: " << pattern_name;
      continue;
    }
    
    BehaviorPattern pattern;
    pattern.name = pattern_name;
    
    const base::Value::Dict& pattern_dict = pattern_value.GetDict();
    const std::string* description = pattern_dict.FindString("description");
    if (description) {
      pattern.description = *description;
    }
    
    // Parse mouse behavior
    const base::Value::Dict* mouse = pattern_dict.FindDict("mouse");
    if (mouse) {
      std::optional<double> movement_speed = mouse->FindDouble("movement_speed");
      if (movement_speed) pattern.mouse.movement_speed = *movement_speed;
      
      std::optional<double> click_delay = mouse->FindDouble("click_delay_ms");
      if (click_delay) pattern.mouse.click_delay_ms = *click_delay;
      
      std::optional<bool> add_random = mouse->FindBool("add_random_movements");
      if (add_random) pattern.mouse.add_random_movements = *add_random;
      
      std::optional<double> random_prob = mouse->FindDouble("random_movement_probability");
      if (random_prob) pattern.mouse.random_movement_probability = *random_prob;
    }
    
    // Parse keyboard behavior
    const base::Value::Dict* keyboard = pattern_dict.FindDict("keyboard");
    if (keyboard) {
      std::optional<double> typing_speed = keyboard->FindDouble("typing_speed_wpm");
      if (typing_speed) pattern.keyboard.typing_speed_wpm = *typing_speed;
      
      std::optional<double> key_delay = keyboard->FindDouble("key_press_delay_ms");
      if (key_delay) pattern.keyboard.key_press_delay_ms = *key_delay;
      
      std::optional<bool> add_errors = keyboard->FindBool("add_typing_errors");
      if (add_errors) pattern.keyboard.add_typing_errors = *add_errors;
      
      std::optional<double> error_prob = keyboard->FindDouble("error_probability");
      if (error_prob) pattern.keyboard.error_probability = *error_prob;
    }
    
    // Parse scroll behavior
    const base::Value::Dict* scroll = pattern_dict.FindDict("scroll");
    if (scroll) {
      std::optional<double> scroll_speed = scroll->FindDouble("scroll_speed");
      if (scroll_speed) pattern.scroll.scroll_speed = *scroll_speed;
      
      std::optional<bool> smooth = scroll->FindBool("smooth_scrolling");
      if (smooth) pattern.scroll.smooth_scrolling = *smooth;
      
      std::optional<double> pause_prob = scroll->FindDouble("pause_probability");
      if (pause_prob) pattern.scroll.pause_probability = *pause_prob;
      
      std::optional<int> pause_duration = scroll->FindInt("pause_duration_ms");
      if (pause_duration) pattern.scroll.pause_duration_ms = *pause_duration;
    }
    
    // Parse interaction behavior
    const base::Value::Dict* interaction = pattern_dict.FindDict("interaction");
    if (interaction) {
      std::optional<double> dwell_time = interaction->FindDouble("page_dwell_time_ms");
      if (dwell_time) pattern.interaction.page_dwell_time_ms = *dwell_time;
      
      std::optional<bool> simulate_reading = interaction->FindBool("simulate_reading");
      if (simulate_reading) pattern.interaction.simulate_reading = *simulate_reading;
      
      std::optional<double> click_prob = interaction->FindDouble("link_click_probability");
      if (click_prob) pattern.interaction.link_click_probability = *click_prob;
      
      std::optional<double> form_speed = interaction->FindDouble("form_fill_speed");
      if (form_speed) pattern.interaction.form_fill_speed = *form_speed;
    }
    
    behavior_patterns_[pattern_name] = std::move(pattern);
  }
  
  LOG(INFO) << "Loaded " << behavior_patterns_.size() << " behavior patterns from: " 
            << patterns_path;
  return true;
}

std::vector<std::string> FingerprintManager::GetAvailablePatterns() const {
  base::AutoLock auto_lock(lock_);
  
  std::vector<std::string> pattern_names;
  pattern_names.reserve(behavior_patterns_.size());
  
  for (const auto& [name, pattern] : behavior_patterns_) {
    pattern_names.push_back(name);
  }
  
  return pattern_names;
}

BehaviorPattern FingerprintManager::GetBehaviorPattern(const std::string& pattern_name) const {
  base::AutoLock auto_lock(lock_);
  
  auto it = behavior_patterns_.find(pattern_name);
  if (it != behavior_patterns_.end()) {
    return it->second;
  }
  
  LOG(WARNING) << "Behavior pattern not found: " << pattern_name;
  return BehaviorPattern{};
}

FingerprintManager::Statistics FingerprintManager::GetStatistics() const {
  base::AutoLock auto_lock(lock_);
  return statistics_;
}

void FingerprintManager::ResetStatistics() {
  base::AutoLock auto_lock(lock_);
  statistics_ = Statistics{};
  LOG(INFO) << "Reset fingerprint protection statistics";
}

void FingerprintManager::IncrementStat(const std::string& stat_name) {
  base::AutoLock auto_lock(lock_);
  
  if (stat_name == "total_frames_protected") {
    statistics_.total_frames_protected++;
  } else if (stat_name == "canvas_operations_spoofed") {
    statistics_.canvas_operations_spoofed++;
  } else if (stat_name == "webgl_parameters_spoofed") {
    statistics_.webgl_parameters_spoofed++;
  } else if (stat_name == "navigator_properties_spoofed") {
    statistics_.navigator_properties_spoofed++;
  } else if (stat_name == "webdriver_detections_blocked") {
    statistics_.webdriver_detections_blocked++;
  } else if (stat_name == "audio_contexts_protected") {
    statistics_.audio_contexts_protected++;
  } else if (stat_name == "font_enumerations_spoofed") {
    statistics_.font_enumerations_spoofed++;
  } else if (stat_name == "geolocation_requests_spoofed") {
    statistics_.geolocation_requests_spoofed++;
  } else if (stat_name == "webrtc_connections_protected") {
    statistics_.webrtc_connections_protected++;
  }
}

void FingerprintManager::InitializeDefaultConfig() {
  default_config_.enabled = true;
  default_config_.profile_name = "default";
  default_config_.device_profile = "windows_desktop";
  default_config_.behavior_pattern = "normal_user";
  default_config_.version = "1.0.0";
  default_config_.created_at = base::Time::Now().ToJsTimeIgnoringNull();
  default_config_.updated_at = default_config_.created_at;
  
  // Initialize Canvas config
  default_config_.canvas.enabled = true;
  default_config_.canvas.add_noise = true;
  default_config_.canvas.noise_level = 0.1;
  default_config_.canvas.spoof_text_metrics = true;
  default_config_.canvas.protect_data_url = true;
  default_config_.canvas.protect_image_data = true;
  
  // Initialize WebGL config
  default_config_.webgl.enabled = true;
  default_config_.webgl.vendor = "Google Inc. (Intel)";
  default_config_.webgl.renderer = "ANGLE (Intel, Intel(R) UHD Graphics 620 Direct3D11 vs_5_0 ps_5_0, D3D11)";
  default_config_.webgl.version = "OpenGL ES 2.0 (ANGLE 2.1.0.0)";
  default_config_.webgl.shading_language_version = "OpenGL ES GLSL ES 1.00 (ANGLE 2.1.0.0)";
  default_config_.webgl.add_noise_to_buffers = true;
  default_config_.webgl.buffer_noise_level = 0.01;
  
  // Initialize Navigator config
  default_config_.navigator.enabled = true;
  default_config_.navigator.user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
  default_config_.navigator.platform = "Win32";
  default_config_.navigator.languages = {"en-US", "en"};
  default_config_.navigator.hardware_concurrency = 8;
  default_config_.navigator.device_memory = 8;
  default_config_.navigator.hide_webdriver = true;
  default_config_.navigator.spoof_plugins = true;
  
  // Initialize Audio config
  default_config_.audio.enabled = true;
  default_config_.audio.add_noise = true;
  default_config_.audio.noise_level = 0.001;
  default_config_.audio.protect_analyser_node = true;
  default_config_.audio.protect_offline_context = true;
  default_config_.audio.sample_rate = 44100;
  default_config_.audio.buffer_size = 4096;
  
  // Initialize Font config
  default_config_.font.enabled = true;
  default_config_.font.spoof_enumeration = true;
  default_config_.font.spoof_metrics = true;
  default_config_.font.available_fonts = {
    "Arial", "Arial Black", "Calibri", "Cambria", "Comic Sans MS",
    "Consolas", "Courier New", "Georgia", "Impact", "Lucida Console",
    "Lucida Sans Unicode", "Microsoft Sans Serif", "Palatino Linotype",
    "Segoe UI", "Tahoma", "Times New Roman", "Trebuchet MS", "Verdana"
  };
  
  // Initialize WebRTC config
  default_config_.webrtc.enabled = true;
  default_config_.webrtc.mask_local_ips = true;
  default_config_.webrtc.disable_webrtc = false;
  default_config_.webrtc.fake_public_ip = "203.0.113.1";
  default_config_.webrtc.block_device_enumeration = true;
  
  // Initialize Geolocation config
  default_config_.geolocation.enabled = true;
  default_config_.geolocation.spoof_location = true;
  default_config_.geolocation.latitude = 40.7128;
  default_config_.geolocation.longitude = -74.0060;
  default_config_.geolocation.accuracy = 10.0;
  default_config_.geolocation.block_high_accuracy = true;
  
  // Initialize Screen config
  default_config_.screen.enabled = true;
  default_config_.screen.width = 1920;
  default_config_.screen.height = 1080;
  default_config_.screen.color_depth = 24;
  default_config_.screen.pixel_depth = 24;
  default_config_.screen.device_pixel_ratio = 1.0;
  default_config_.screen.orientation = "landscape-primary";
  
  // Initialize Timezone config
  default_config_.timezone.enabled = true;
  default_config_.timezone.timezone = "America/New_York";
  default_config_.timezone.timezone_offset = -300;
  default_config_.timezone.spoof_date_methods = true;
  
  // Initialize Anti-detection config
  default_config_.anti_detection.enabled = true;
  default_config_.anti_detection.webdriver.hide_webdriver_property = true;
  default_config_.anti_detection.webdriver.hide_automation_flags = true;
  default_config_.anti_detection.webdriver.spoof_chrome_runtime = true;
  default_config_.anti_detection.webdriver.hide_selenium_variables = true;
  default_config_.anti_detection.webdriver.blocked_properties = {
    "webdriver", "__webdriver_evaluate", "__selenium_evaluate",
    "__webdriver_script_function", "__webdriver_script_func",
    "__webdriver_script_fn", "__fxdriver_evaluate", "__driver_unwrapped",
    "webdriver_id", "$chrome_asyncScriptInfo", "$cdc_asdjflasutopfhvcZLmcfl_"
  };
  
  default_config_.anti_detection.automation.hide_headless_flags = true;
  default_config_.anti_detection.automation.spoof_user_interaction = true;
  default_config_.anti_detection.automation.add_human_delays = true;
  default_config_.anti_detection.automation.randomize_request_timing = true;
  default_config_.anti_detection.automation.min_delay_ms = 100;
  default_config_.anti_detection.automation.max_delay_ms = 2000;
  
  default_config_.anti_detection.js_injection.detect_puppeteer = true;
  default_config_.anti_detection.js_injection.detect_playwright = true;
  default_config_.anti_detection.js_injection.detect_selenium = true;
  default_config_.anti_detection.js_injection.block_detection_scripts = true;
  default_config_.anti_detection.js_injection.blocked_script_patterns = {
    "puppeteer", "playwright", "selenium", "webdriver", "automation",
    "headless", "__nightmare", "_phantom", "callPhantom"
  };
  
  // Initialize custom JS injections
  default_config_.custom_js_injections = {
    "Object.defineProperty(navigator, 'webdriver', {get: () => undefined});",
    "delete window.cdc_adoQpoasnfa76pfcZLmcfl_Array;",
    "delete window.cdc_adoQpoasnfa76pfcZLmcfl_Promise;",
    "delete window.cdc_adoQpoasnfa76pfcZLmcfl_Symbol;"
  };
}

std::string FingerprintManager::GenerateFrameId(content::RenderFrameHost* frame) {
  if (!frame) {
    return "null_frame";
  }
  
  content::WebContents* web_contents = content::WebContents::FromRenderFrameHost(frame);
  if (!web_contents) {
    return base::StringPrintf("frame_%d_%d", 
                             frame->GetProcess()->GetID(),
                             frame->GetRoutingID());
  }
  
  return base::StringPrintf("frame_%d_%d_%d",
                           web_contents->GetBrowserContext()->GetPath().BaseName().MaybeAsASCII().c_str()[0],
                           frame->GetProcess()->GetID(),
                           frame->GetRoutingID());
}

}  // namespace novebrowse
