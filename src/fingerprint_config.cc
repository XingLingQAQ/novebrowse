#include "novebrowse/fingerprint_config.h"

#include <sstream>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "crypto/sha2.h"

namespace novebrowse {

// FingerprintConfig implementation
mojom::FingerprintConfigPtr FingerprintConfig::ToMojoStruct() const {
  auto mojo_config = mojom::FingerprintConfig::New();
  
  mojo_config->enabled = enabled;
  mojo_config->profile_name = profile_name;
  mojo_config->device_profile = device_profile;
  mojo_config->behavior_pattern = behavior_pattern;
  mojo_config->created_at = created_at;
  mojo_config->updated_at = updated_at;
  mojo_config->version = version;
  
  // Canvas config
  mojo_config->canvas = mojom::CanvasConfig::New();
  mojo_config->canvas->enabled = canvas.enabled;
  mojo_config->canvas->add_noise = canvas.add_noise;
  mojo_config->canvas->noise_level = canvas.noise_level;
  mojo_config->canvas->spoof_text_metrics = canvas.spoof_text_metrics;
  mojo_config->canvas->protect_data_url = canvas.protect_data_url;
  mojo_config->canvas->protect_image_data = canvas.protect_image_data;
  
  // WebGL config
  mojo_config->webgl = mojom::WebGLConfig::New();
  mojo_config->webgl->enabled = webgl.enabled;
  mojo_config->webgl->vendor = webgl.vendor;
  mojo_config->webgl->renderer = webgl.renderer;
  mojo_config->webgl->version = webgl.version;
  mojo_config->webgl->shading_language_version = webgl.shading_language_version;
  mojo_config->webgl->extensions = webgl.extensions;
  mojo_config->webgl->parameters = webgl.parameters;
  mojo_config->webgl->add_noise_to_buffers = webgl.add_noise_to_buffers;
  mojo_config->webgl->buffer_noise_level = webgl.buffer_noise_level;
  
  // Navigator config
  mojo_config->navigator = mojom::NavigatorConfig::New();
  mojo_config->navigator->enabled = navigator.enabled;
  mojo_config->navigator->user_agent = navigator.user_agent;
  mojo_config->navigator->platform = navigator.platform;
  mojo_config->navigator->languages = navigator.languages;
  mojo_config->navigator->hardware_concurrency = navigator.hardware_concurrency;
  mojo_config->navigator->device_memory = navigator.device_memory;
  mojo_config->navigator->hide_webdriver = navigator.hide_webdriver;
  mojo_config->navigator->spoof_plugins = navigator.spoof_plugins;
  mojo_config->navigator->mime_types = navigator.mime_types;
  
  // Audio config
  mojo_config->audio = mojom::AudioConfig::New();
  mojo_config->audio->enabled = audio.enabled;
  mojo_config->audio->add_noise = audio.add_noise;
  mojo_config->audio->noise_level = audio.noise_level;
  mojo_config->audio->protect_analyser_node = audio.protect_analyser_node;
  mojo_config->audio->protect_offline_context = audio.protect_offline_context;
  mojo_config->audio->sample_rate = audio.sample_rate;
  mojo_config->audio->buffer_size = audio.buffer_size;
  
  // Font config
  mojo_config->font = mojom::FontConfig::New();
  mojo_config->font->enabled = font.enabled;
  mojo_config->font->spoof_enumeration = font.spoof_enumeration;
  mojo_config->font->spoof_metrics = font.spoof_metrics;
  mojo_config->font->available_fonts = font.available_fonts;
  mojo_config->font->font_metrics_offsets = font.font_metrics_offsets;
  
  // WebRTC config
  mojo_config->webrtc = mojom::WebRTCConfig::New();
  mojo_config->webrtc->enabled = webrtc.enabled;
  mojo_config->webrtc->mask_local_ips = webrtc.mask_local_ips;
  mojo_config->webrtc->disable_webrtc = webrtc.disable_webrtc;
  mojo_config->webrtc->fake_public_ip = webrtc.fake_public_ip;
  mojo_config->webrtc->allowed_ice_servers = webrtc.allowed_ice_servers;
  mojo_config->webrtc->block_device_enumeration = webrtc.block_device_enumeration;
  
  // Geolocation config
  mojo_config->geolocation = mojom::GeolocationConfig::New();
  mojo_config->geolocation->enabled = geolocation.enabled;
  mojo_config->geolocation->spoof_location = geolocation.spoof_location;
  mojo_config->geolocation->latitude = geolocation.latitude;
  mojo_config->geolocation->longitude = geolocation.longitude;
  mojo_config->geolocation->accuracy = geolocation.accuracy;
  mojo_config->geolocation->block_high_accuracy = geolocation.block_high_accuracy;
  
  // Screen config
  mojo_config->screen = mojom::ScreenConfig::New();
  mojo_config->screen->enabled = screen.enabled;
  mojo_config->screen->width = screen.width;
  mojo_config->screen->height = screen.height;
  mojo_config->screen->color_depth = screen.color_depth;
  mojo_config->screen->pixel_depth = screen.pixel_depth;
  mojo_config->screen->device_pixel_ratio = screen.device_pixel_ratio;
  mojo_config->screen->orientation = screen.orientation;
  
  // Timezone config
  mojo_config->timezone = mojom::TimezoneConfig::New();
  mojo_config->timezone->enabled = timezone.enabled;
  mojo_config->timezone->timezone = timezone.timezone;
  mojo_config->timezone->timezone_offset = timezone.timezone_offset;
  mojo_config->timezone->spoof_date_methods = timezone.spoof_date_methods;
  
  // Anti-detection config
  mojo_config->anti_detection = mojom::AntiDetectionConfig::New();
  mojo_config->anti_detection->enabled = anti_detection.enabled;
  
  mojo_config->anti_detection->webdriver = mojom::WebDriverProtection::New();
  mojo_config->anti_detection->webdriver->hide_webdriver_property = anti_detection.webdriver.hide_webdriver_property;
  mojo_config->anti_detection->webdriver->hide_automation_flags = anti_detection.webdriver.hide_automation_flags;
  mojo_config->anti_detection->webdriver->spoof_chrome_runtime = anti_detection.webdriver.spoof_chrome_runtime;
  mojo_config->anti_detection->webdriver->hide_selenium_variables = anti_detection.webdriver.hide_selenium_variables;
  mojo_config->anti_detection->webdriver->blocked_properties = anti_detection.webdriver.blocked_properties;
  
  mojo_config->anti_detection->automation = mojom::AutomationProtection::New();
  mojo_config->anti_detection->automation->hide_headless_flags = anti_detection.automation.hide_headless_flags;
  mojo_config->anti_detection->automation->spoof_user_interaction = anti_detection.automation.spoof_user_interaction;
  mojo_config->anti_detection->automation->add_human_delays = anti_detection.automation.add_human_delays;
  mojo_config->anti_detection->automation->randomize_request_timing = anti_detection.automation.randomize_request_timing;
  mojo_config->anti_detection->automation->min_delay_ms = anti_detection.automation.min_delay_ms;
  mojo_config->anti_detection->automation->max_delay_ms = anti_detection.automation.max_delay_ms;
  
  mojo_config->anti_detection->js_injection = mojom::JSInjectionProtection::New();
  mojo_config->anti_detection->js_injection->detect_puppeteer = anti_detection.js_injection.detect_puppeteer;
  mojo_config->anti_detection->js_injection->detect_playwright = anti_detection.js_injection.detect_playwright;
  mojo_config->anti_detection->js_injection->detect_selenium = anti_detection.js_injection.detect_selenium;
  mojo_config->anti_detection->js_injection->block_detection_scripts = anti_detection.js_injection.block_detection_scripts;
  mojo_config->anti_detection->js_injection->blocked_script_patterns = anti_detection.js_injection.blocked_script_patterns;
  
  mojo_config->custom_js_injections = custom_js_injections;
  
  return mojo_config;
}

FingerprintConfig FingerprintConfig::FromMojoStruct(const mojom::FingerprintConfigPtr& mojo_config) {
  FingerprintConfig config;
  
  if (!mojo_config) {
    return config;
  }
  
  config.enabled = mojo_config->enabled;
  config.profile_name = mojo_config->profile_name;
  config.device_profile = mojo_config->device_profile;
  config.behavior_pattern = mojo_config->behavior_pattern;
  config.created_at = mojo_config->created_at;
  config.updated_at = mojo_config->updated_at;
  config.version = mojo_config->version;
  
  // Canvas config
  if (mojo_config->canvas) {
    config.canvas.enabled = mojo_config->canvas->enabled;
    config.canvas.add_noise = mojo_config->canvas->add_noise;
    config.canvas.noise_level = mojo_config->canvas->noise_level;
    config.canvas.spoof_text_metrics = mojo_config->canvas->spoof_text_metrics;
    config.canvas.protect_data_url = mojo_config->canvas->protect_data_url;
    config.canvas.protect_image_data = mojo_config->canvas->protect_image_data;
  }
  
  // WebGL config
  if (mojo_config->webgl) {
    config.webgl.enabled = mojo_config->webgl->enabled;
    config.webgl.vendor = mojo_config->webgl->vendor;
    config.webgl.renderer = mojo_config->webgl->renderer;
    config.webgl.version = mojo_config->webgl->version;
    config.webgl.shading_language_version = mojo_config->webgl->shading_language_version;
    config.webgl.extensions = mojo_config->webgl->extensions;
    config.webgl.parameters = mojo_config->webgl->parameters;
    config.webgl.add_noise_to_buffers = mojo_config->webgl->add_noise_to_buffers;
    config.webgl.buffer_noise_level = mojo_config->webgl->buffer_noise_level;
  }
  
  // Navigator config
  if (mojo_config->navigator) {
    config.navigator.enabled = mojo_config->navigator->enabled;
    config.navigator.user_agent = mojo_config->navigator->user_agent;
    config.navigator.platform = mojo_config->navigator->platform;
    config.navigator.languages = mojo_config->navigator->languages;
    config.navigator.hardware_concurrency = mojo_config->navigator->hardware_concurrency;
    config.navigator.device_memory = mojo_config->navigator->device_memory;
    config.navigator.hide_webdriver = mojo_config->navigator->hide_webdriver;
    config.navigator.spoof_plugins = mojo_config->navigator->spoof_plugins;
    config.navigator.mime_types = mojo_config->navigator->mime_types;
  }
  
  // Audio config
  if (mojo_config->audio) {
    config.audio.enabled = mojo_config->audio->enabled;
    config.audio.add_noise = mojo_config->audio->add_noise;
    config.audio.noise_level = mojo_config->audio->noise_level;
    config.audio.protect_analyser_node = mojo_config->audio->protect_analyser_node;
    config.audio.protect_offline_context = mojo_config->audio->protect_offline_context;
    config.audio.sample_rate = mojo_config->audio->sample_rate;
    config.audio.buffer_size = mojo_config->audio->buffer_size;
  }
  
  // Font config
  if (mojo_config->font) {
    config.font.enabled = mojo_config->font->enabled;
    config.font.spoof_enumeration = mojo_config->font->spoof_enumeration;
    config.font.spoof_metrics = mojo_config->font->spoof_metrics;
    config.font.available_fonts = mojo_config->font->available_fonts;
    config.font.font_metrics_offsets = mojo_config->font->font_metrics_offsets;
  }
  
  // WebRTC config
  if (mojo_config->webrtc) {
    config.webrtc.enabled = mojo_config->webrtc->enabled;
    config.webrtc.mask_local_ips = mojo_config->webrtc->mask_local_ips;
    config.webrtc.disable_webrtc = mojo_config->webrtc->disable_webrtc;
    config.webrtc.fake_public_ip = mojo_config->webrtc->fake_public_ip;
    config.webrtc.allowed_ice_servers = mojo_config->webrtc->allowed_ice_servers;
    config.webrtc.block_device_enumeration = mojo_config->webrtc->block_device_enumeration;
  }
  
  // Geolocation config
  if (mojo_config->geolocation) {
    config.geolocation.enabled = mojo_config->geolocation->enabled;
    config.geolocation.spoof_location = mojo_config->geolocation->spoof_location;
    config.geolocation.latitude = mojo_config->geolocation->latitude;
    config.geolocation.longitude = mojo_config->geolocation->longitude;
    config.geolocation.accuracy = mojo_config->geolocation->accuracy;
    config.geolocation.block_high_accuracy = mojo_config->geolocation->block_high_accuracy;
  }
  
  // Screen config
  if (mojo_config->screen) {
    config.screen.enabled = mojo_config->screen->enabled;
    config.screen.width = mojo_config->screen->width;
    config.screen.height = mojo_config->screen->height;
    config.screen.color_depth = mojo_config->screen->color_depth;
    config.screen.pixel_depth = mojo_config->screen->pixel_depth;
    config.screen.device_pixel_ratio = mojo_config->screen->device_pixel_ratio;
    config.screen.orientation = mojo_config->screen->orientation;
  }
  
  // Timezone config
  if (mojo_config->timezone) {
    config.timezone.enabled = mojo_config->timezone->enabled;
    config.timezone.timezone = mojo_config->timezone->timezone;
    config.timezone.timezone_offset = mojo_config->timezone->timezone_offset;
    config.timezone.spoof_date_methods = mojo_config->timezone->spoof_date_methods;
  }
  
  // Anti-detection config
  if (mojo_config->anti_detection) {
    config.anti_detection.enabled = mojo_config->anti_detection->enabled;
    
    if (mojo_config->anti_detection->webdriver) {
      config.anti_detection.webdriver.hide_webdriver_property = mojo_config->anti_detection->webdriver->hide_webdriver_property;
      config.anti_detection.webdriver.hide_automation_flags = mojo_config->anti_detection->webdriver->hide_automation_flags;
      config.anti_detection.webdriver.spoof_chrome_runtime = mojo_config->anti_detection->webdriver->spoof_chrome_runtime;
      config.anti_detection.webdriver.hide_selenium_variables = mojo_config->anti_detection->webdriver->hide_selenium_variables;
      config.anti_detection.webdriver.blocked_properties = mojo_config->anti_detection->webdriver->blocked_properties;
    }
    
    if (mojo_config->anti_detection->automation) {
      config.anti_detection.automation.hide_headless_flags = mojo_config->anti_detection->automation->hide_headless_flags;
      config.anti_detection.automation.spoof_user_interaction = mojo_config->anti_detection->automation->spoof_user_interaction;
      config.anti_detection.automation.add_human_delays = mojo_config->anti_detection->automation->add_human_delays;
      config.anti_detection.automation.randomize_request_timing = mojo_config->anti_detection->automation->randomize_request_timing;
      config.anti_detection.automation.min_delay_ms = mojo_config->anti_detection->automation->min_delay_ms;
      config.anti_detection.automation.max_delay_ms = mojo_config->anti_detection->automation->max_delay_ms;
    }
    
    if (mojo_config->anti_detection->js_injection) {
      config.anti_detection.js_injection.detect_puppeteer = mojo_config->anti_detection->js_injection->detect_puppeteer;
      config.anti_detection.js_injection.detect_playwright = mojo_config->anti_detection->js_injection->detect_playwright;
      config.anti_detection.js_injection.detect_selenium = mojo_config->anti_detection->js_injection->detect_selenium;
      config.anti_detection.js_injection.block_detection_scripts = mojo_config->anti_detection->js_injection->block_detection_scripts;
      config.anti_detection.js_injection.blocked_script_patterns = mojo_config->anti_detection->js_injection->blocked_script_patterns;
    }
  }
  
  config.custom_js_injections = mojo_config->custom_js_injections;
  
  return config;
}

base::Value FingerprintConfig::ToValue() const {
  base::Value::Dict config_dict;
  
  config_dict.Set("enabled", enabled);
  config_dict.Set("profile_name", profile_name);
  config_dict.Set("device_profile", device_profile);
  config_dict.Set("behavior_pattern", behavior_pattern);
  config_dict.Set("created_at", created_at);
  config_dict.Set("updated_at", updated_at);
  config_dict.Set("version", version);
  
  // Canvas config
  base::Value::Dict canvas_dict;
  canvas_dict.Set("enabled", canvas.enabled);
  canvas_dict.Set("add_noise", canvas.add_noise);
  canvas_dict.Set("noise_level", canvas.noise_level);
  canvas_dict.Set("spoof_text_metrics", canvas.spoof_text_metrics);
  canvas_dict.Set("protect_data_url", canvas.protect_data_url);
  canvas_dict.Set("protect_image_data", canvas.protect_image_data);
  config_dict.Set("canvas", std::move(canvas_dict));
  
  // WebGL config
  base::Value::Dict webgl_dict;
  webgl_dict.Set("enabled", webgl.enabled);
  webgl_dict.Set("vendor", webgl.vendor);
  webgl_dict.Set("renderer", webgl.renderer);
  webgl_dict.Set("version", webgl.version);
  webgl_dict.Set("shading_language_version", webgl.shading_language_version);
  
  base::Value::List extensions_list;
  for (const auto& ext : webgl.extensions) {
    extensions_list.Append(ext);
  }
  webgl_dict.Set("extensions", std::move(extensions_list));
  
  base::Value::Dict parameters_dict;
  for (const auto& [key, value] : webgl.parameters) {
    parameters_dict.Set(key, value);
  }
  webgl_dict.Set("parameters", std::move(parameters_dict));
  
  webgl_dict.Set("add_noise_to_buffers", webgl.add_noise_to_buffers);
  webgl_dict.Set("buffer_noise_level", webgl.buffer_noise_level);
  config_dict.Set("webgl", std::move(webgl_dict));
  
  // Navigator config
  base::Value::Dict navigator_dict;
  navigator_dict.Set("enabled", navigator.enabled);
  navigator_dict.Set("user_agent", navigator.user_agent);
  navigator_dict.Set("platform", navigator.platform);
  
  base::Value::List languages_list;
  for (const auto& lang : navigator.languages) {
    languages_list.Append(lang);
  }
  navigator_dict.Set("languages", std::move(languages_list));
  
  navigator_dict.Set("hardware_concurrency", navigator.hardware_concurrency);
  navigator_dict.Set("device_memory", static_cast<double>(navigator.device_memory));
  navigator_dict.Set("hide_webdriver", navigator.hide_webdriver);
  navigator_dict.Set("spoof_plugins", navigator.spoof_plugins);
  
  base::Value::List mime_types_list;
  for (const auto& mime : navigator.mime_types) {
    mime_types_list.Append(mime);
  }
  navigator_dict.Set("mime_types", std::move(mime_types_list));
  config_dict.Set("navigator", std::move(navigator_dict));
  
  // Custom JS injections
  base::Value::List js_injections_list;
  for (const auto& injection : custom_js_injections) {
    js_injections_list.Append(injection);
  }
  config_dict.Set("custom_js_injections", std::move(js_injections_list));
  
  return base::Value(std::move(config_dict));
}

FingerprintConfig FingerprintConfig::FromValue(const base::Value& value) {
  FingerprintConfig config;
  
  if (!value.is_dict()) {
    return config;
  }
  
  const base::Value::Dict& dict = value.GetDict();
  
  const std::optional<bool> enabled_opt = dict.FindBool("enabled");
  if (enabled_opt) config.enabled = *enabled_opt;
  
  const std::string* profile_name = dict.FindString("profile_name");
  if (profile_name) config.profile_name = *profile_name;
  
  const std::string* device_profile = dict.FindString("device_profile");
  if (device_profile) config.device_profile = *device_profile;
  
  const std::string* behavior_pattern = dict.FindString("behavior_pattern");
  if (behavior_pattern) config.behavior_pattern = *behavior_pattern;
  
  const std::string* created_at = dict.FindString("created_at");
  if (created_at) config.created_at = *created_at;
  
  const std::string* updated_at = dict.FindString("updated_at");
  if (updated_at) config.updated_at = *updated_at;
  
  const std::string* version = dict.FindString("version");
  if (version) config.version = *version;
  
  // Parse canvas config
  const base::Value::Dict* canvas_dict = dict.FindDict("canvas");
  if (canvas_dict) {
    const std::optional<bool> canvas_enabled = canvas_dict->FindBool("enabled");
    if (canvas_enabled) config.canvas.enabled = *canvas_enabled;
    
    const std::optional<bool> add_noise = canvas_dict->FindBool("add_noise");
    if (add_noise) config.canvas.add_noise = *add_noise;
    
    const std::optional<double> noise_level = canvas_dict->FindDouble("noise_level");
    if (noise_level) config.canvas.noise_level = *noise_level;
    
    const std::optional<bool> spoof_text_metrics = canvas_dict->FindBool("spoof_text_metrics");
    if (spoof_text_metrics) config.canvas.spoof_text_metrics = *spoof_text_metrics;
    
    const std::optional<bool> protect_data_url = canvas_dict->FindBool("protect_data_url");
    if (protect_data_url) config.canvas.protect_data_url = *protect_data_url;
    
    const std::optional<bool> protect_image_data = canvas_dict->FindBool("protect_image_data");
    if (protect_image_data) config.canvas.protect_image_data = *protect_image_data;
  }
  
  return config;
}

bool FingerprintConfig::IsValid() const {
  if (profile_name.empty()) {
    return false;
  }
  
  if (navigator.enabled && navigator.user_agent.empty()) {
    return false;
  }
  
  if (screen.enabled && (screen.width <= 0 || screen.height <= 0)) {
    return false;
  }
  
  if (canvas.enabled && (canvas.noise_level < 0.0 || canvas.noise_level > 1.0)) {
    return false;
  }
  
  if (webgl.enabled && (webgl.buffer_noise_level < 0.0 || webgl.buffer_noise_level > 1.0)) {
    return false;
  }
  
  return true;
}

std::vector<std::string> FingerprintConfig::GetValidationErrors() const {
  std::vector<std::string> errors;
  
  if (profile_name.empty()) {
    errors.push_back("Profile name cannot be empty");
  }
  
  if (navigator.enabled && navigator.user_agent.empty()) {
    errors.push_back("User agent cannot be empty when navigator spoofing is enabled");
  }
  
  if (screen.enabled && (screen.width <= 0 || screen.height <= 0)) {
    errors.push_back("Screen dimensions must be positive when screen spoofing is enabled");
  }
  
  if (canvas.enabled && (canvas.noise_level < 0.0 || canvas.noise_level > 1.0)) {
    errors.push_back("Canvas noise level must be between 0.0 and 1.0");
  }
  
  if (webgl.enabled && (webgl.buffer_noise_level < 0.0 || webgl.buffer_noise_level > 1.0)) {
    errors.push_back("WebGL buffer noise level must be between 0.0 and 1.0");
  }
  
  if (audio.enabled && (audio.noise_level < 0.0 || audio.noise_level > 1.0)) {
    errors.push_back("Audio noise level must be between 0.0 and 1.0");
  }
  
  return errors;
}

void FingerprintConfig::MergeWith(const FingerprintConfig& other) {
  if (!other.profile_name.empty()) {
    profile_name = other.profile_name;
  }
  
  if (!other.device_profile.empty()) {
    device_profile = other.device_profile;
  }
  
  if (!other.behavior_pattern.empty()) {
    behavior_pattern = other.behavior_pattern;
  }
  
  // Merge canvas config
  if (other.canvas.enabled) {
    canvas = other.canvas;
  }
  
  // Merge WebGL config
  if (other.webgl.enabled) {
    webgl = other.webgl;
  }
  
  // Merge navigator config
  if (other.navigator.enabled) {
    navigator = other.navigator;
  }
  
  // Merge other configs similarly
  if (other.audio.enabled) {
    audio = other.audio;
  }
  
  if (other.font.enabled) {
    font = other.font;
  }
  
  if (other.webrtc.enabled) {
    webrtc = other.webrtc;
  }
  
  if (other.geolocation.enabled) {
    geolocation = other.geolocation;
  }
  
  if (other.screen.enabled) {
    screen = other.screen;
  }
  
  if (other.timezone.enabled) {
    timezone = other.timezone;
  }
  
  if (other.anti_detection.enabled) {
    anti_detection = other.anti_detection;
  }
  
  // Merge custom JS injections
  if (!other.custom_js_injections.empty()) {
    custom_js_injections.insert(custom_js_injections.end(),
                               other.custom_js_injections.begin(),
                               other.custom_js_injections.end());
  }
  
  updated_at = base::Time::Now().ToJsTimeIgnoringNull();
}

std::string FingerprintConfig::GetConfigHash() const {
  base::Value config_value = ToValue();
  std::string config_json;
  base::JSONWriter::Write(config_value, &config_json);
  
  std::string hash = crypto::SHA256HashString(config_json);
  return base::HexEncode(hash.data(), hash.size());
}
