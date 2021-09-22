#include "ons/ONSFactoryProperty.h"

#include <fstream>
#include <ios>
#include <string>
#include <system_error>

#include "absl/strings/numbers.h"
#include "absl/strings/str_join.h"
#include "ghc/filesystem.hpp"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/util/json_util.h"
#include "spdlog/spdlog.h"

#include "FAQ.h"
#include "MixAll.h"
#include "ons/ONSClientException.h"

namespace ons {

const char* ONSFactoryProperty::LogPath = "LogPath";
const char* ONSFactoryProperty::ProducerId = "ProducerId";
const char* ONSFactoryProperty::ConsumerId = "ConsumerId";
const char* ONSFactoryProperty::GroupId = "GroupId";
const char* ONSFactoryProperty::AccessKey = "AccessKey";
const char* ONSFactoryProperty::SecretKey = "SecretKey";
const char* ONSFactoryProperty::MessageModel = "MessageModel";
const char* ONSFactoryProperty::BROADCASTING = "BROADCASTING";
const char* ONSFactoryProperty::CLUSTERING = "CLUSTERING";
const char* ONSFactoryProperty::SendMsgTimeoutMillis = "SendMsgTimeoutMillis";
const char* ONSFactoryProperty::SuspendTimeMillis = "SuspendTimeMillis";
const char* ONSFactoryProperty::SendMsgRetryTimes = "SendMsgRetryTimes";
const char* ONSFactoryProperty::MaxMsgCacheSize = "MaxMsgCacheSize";
const char* ONSFactoryProperty::MaxCachedMessageSizeInMiB = "MaxCachedMessageSizeInMiB";
const char* ONSFactoryProperty::ONSAddr = "ONSAddr";           // name server domain name
const char* ONSFactoryProperty::NAMESRV_ADDR = "NAMESRV_ADDR"; // name server ip addr
const char* ONSFactoryProperty::ConsumeThreadNums = "ConsumeThreadNums";
const char* ONSFactoryProperty::OnsChannel = "OnsChannel";
const char* ONSFactoryProperty::OnsTraceSwitch = "OnsTraceSwitch";
const char* ONSFactoryProperty::ConsumerInstanceName = "ConsumerInstanceName";
const char* ONSFactoryProperty::InstanceId = "InstanceId";
const char* ONSFactoryProperty::DEFAULT_CHANNEL = "ALIYUN";

const std::string ONSFactoryProperty::EMPTY_STRING;

ONSFactoryProperty::ONSFactoryProperty() {
  setDefaults();
  loadConfigFile();
}

void ONSFactoryProperty::setDefaults() {
  setMessageModel(MessageModel::CLUSTERING);
  setSendMsgTimeout(std::chrono::seconds(3));
  setSuspendDuration(std::chrono::seconds(3));
  setFactoryProperty(MaxMsgCacheSize, "1000");
  this->withTraceFeature(Trace::ON);
}

void ONSFactoryProperty::loadConfigFile() {
  std::string home_directory;
  if (!ROCKETMQ_NAMESPACE::MixAll::homeDirectory(home_directory)) {
    return;
  }

  std::vector<std::string> config_file_path_segments = {home_directory, "ons", "credential"};

  std::string path_separator;
  path_separator.push_back(ghc::filesystem::path::preferred_separator);
  std::string config_path_string = absl::StrJoin(config_file_path_segments, path_separator);
  ghc::filesystem::path config_file_path(config_path_string);

  std::error_code ec;
  if (!ghc::filesystem::exists(config_file_path, ec) || !ghc::filesystem::is_regular_file(config_file_path, ec)) {
    SPDLOG_INFO("No default config file found at {}", config_file_path.c_str());
    return;
  }

  std::fstream config_file_stream;
  config_file_stream.open(config_file_path.c_str(), std::ios_base::in);
  if (!config_file_stream.is_open() || !config_file_stream.good()) {
    SPDLOG_WARN("Failed to read config file: {}", config_file_path.c_str());
    return;
  }

  std::string json;

  std::string line;
  while (std::getline(config_file_stream, line)) {
    json.append(line).append("\n");
  }
  config_file_stream.close();

  google::protobuf::Struct root;
  google::protobuf::util::Status status = google::protobuf::util::JsonStringToMessage(json, &root);
  if (!status.ok()) {
    SPDLOG_WARN("Failed to parse config JSON. Cause: {}", status.message().as_string());
    return;
  }

  auto fields = root.fields();
  if (fields.contains(AccessKey)) {
    setFactoryProperty(AccessKey, fields[AccessKey].string_value());
    SPDLOG_INFO("Set {} through default config file", AccessKey);
  }

  if (fields.contains(SecretKey)) {
    setFactoryProperty(SecretKey, fields[SecretKey].string_value());
    SPDLOG_INFO("Set {} through default config file", SecretKey);
  }

  if (fields.contains(NAMESRV_ADDR)) {
    setFactoryProperty(NAMESRV_ADDR, fields[NAMESRV_ADDR].string_value());
    SPDLOG_INFO("Set {} through default config file", NAMESRV_ADDR);
  }

  if (fields.contains(GroupId)) {
    setFactoryProperty(GroupId, fields[GroupId].string_value());
    SPDLOG_INFO("Set {} through default config file", GroupId);
  }
}

bool ONSFactoryProperty::validate(const std::string& key, const std::string& value) {
  if (key == MessageModel) {
    if (value != BROADCASTING && value != CLUSTERING) {
      throw ONSClientException(FAQ::errorMessage("MessageModel could only be set to BROADCASTING "
                                                 "or CLUSTERING, please set it.",
                                                 FAQ::CLIENT_CHECK_MSG_EXCEPTION));
    }
  }

  if (key == AccessKey) {
    if (value.empty()) {
      throw ONSClientException(FAQ::errorMessage("AccessKey must be set.", FAQ::CLIENT_CHECK_MSG_EXCEPTION));
    }
  }

  if (key == SecretKey) {
    if (value.empty()) {
      throw ONSClientException(FAQ::errorMessage("SecretKey must be set.", FAQ::CLIENT_CHECK_MSG_EXCEPTION));
    }
  }
  return true;
}

std::string ONSFactoryProperty::getLogPath() const {
  auto optional = getProperty(LogPath);
  return optional.value_or(std::string());
}

ONSFactoryProperty& ONSFactoryProperty::setSendMsgTimeout(int value) {
  char tmp[16];
  sprintf(tmp, "%d", value);
  setFactoryProperty(SendMsgTimeoutMillis, tmp);
  return *this;
}

ONSFactoryProperty& ONSFactoryProperty::setSendMsgTimeout(std::chrono::milliseconds timeout) {
  setFactoryProperty(SendMsgTimeoutMillis, std::to_string(timeout.count()));
  return *this;
}

ONSFactoryProperty& ONSFactoryProperty::setSendMsgRetryTimes(int value) {
  char tmp[16];
  sprintf(tmp, "%d", value);
  setFactoryProperty(SendMsgRetryTimes, tmp);
  return *this;
}

ONSFactoryProperty& ONSFactoryProperty::setMaxMsgCacheSize(int value) {
  char tmp[256] = {0};
  sprintf(tmp, "%d", value);
  setFactoryProperty(MaxMsgCacheSize, tmp);
  return *this;
}

ONSFactoryProperty& ONSFactoryProperty::withTraceFeature(Trace trace_flag) {
  switch (trace_flag) {
  case Trace::ON:
    setFactoryProperty(OnsTraceSwitch, "true");
    break;

  case Trace::OFF:
    setFactoryProperty(OnsTraceSwitch, "false");
    break;
  }
  return *this;
}

ONSFactoryProperty& ONSFactoryProperty::setOnsTraceSwitch(bool should_trace) {
  if (should_trace) {
    setFactoryProperty(OnsTraceSwitch, "true");
  } else {
    setFactoryProperty(OnsTraceSwitch, "false");
  }
  return *this;
}

void ONSFactoryProperty::setOnsChannel(ONSChannel channel) {
  if (channel == ONSChannel::CLOUD) {
    setFactoryProperty(OnsChannel, "CLOUD");
  } else if (channel == ONSChannel::ALIYUN) {
    setFactoryProperty(OnsChannel, "ALIYUN");
  } else if (channel == ONSChannel::ALL) {
    setFactoryProperty(OnsChannel, "ALL");
  } else if (channel == ONSChannel::LOCAL) {
    setFactoryProperty(OnsChannel, "LOCAL");
  } else if (channel == ONSChannel::INNER) {
    setFactoryProperty(OnsChannel, "INNER");
  } else {
    throw ONSClientException(FAQ::errorMessage("ONSChannel could only be set to "
                                               "CLOUD/ALIYUN/ALL, please reset it.",
                                               FAQ::CLIENT_CHECK_MSG_EXCEPTION));
  }
}

void ONSFactoryProperty::setFactoryProperty(absl::string_view key, absl::string_view value) {

  std::string keyString(key.data(), key.length());
  std::string valueString(value.data(), value.length());

  try {
    if (validate(keyString, valueString)) {
      property_map_[keyString] = valueString;
    }
  } catch (ONSClientException& e) {
    throw e;
  }
}

absl::optional<std::string> ONSFactoryProperty::getProperty(absl::string_view key) const {
  std::string k(key.data(), key.length());
  auto it = property_map_.find(k);
  if (property_map_.end() == it) {
    return absl::optional<std::string>();
  }
  return absl::make_optional<std::string>(it->second);
}

std::string ONSFactoryProperty::getProperty(absl::string_view key, std::string default_value) const {
  return getProperty(key).value_or(std::move(default_value));
}

void ONSFactoryProperty::setFactoryProperties(const std::map<std::string, std::string>& factory_properties) {
  property_map_ = factory_properties;
}

std::map<std::string, std::string> ONSFactoryProperty::getFactoryProperties() const { return property_map_; }

std::string ONSFactoryProperty::getProducerId() const {
  absl::optional<std::string> group_id = getProperty(GroupId);
  if (group_id.has_value()) {
    return group_id.value();
  }

  return getProperty(ProducerId, EMPTY_STRING);
}

std::string ONSFactoryProperty::getConsumerId() const {
  absl::optional<std::string> group_id = getProperty(GroupId);
  if (group_id.has_value()) {
    return group_id.value();
  }
  return getProperty(ConsumerId, EMPTY_STRING);
}

std::string ONSFactoryProperty::getGroupId() const { return getProperty(GroupId, EMPTY_STRING); }

std::string ONSFactoryProperty::getMessageModel() const { return getProperty(MessageModel, EMPTY_STRING); }

ONSFactoryProperty& ONSFactoryProperty::setMessageModel(ons::MessageModel message_model) {
  switch (message_model) {
  case MessageModel::CLUSTERING:
    setFactoryProperty(MessageModel, CLUSTERING);
    break;

  case MessageModel::BROADCASTING:
    setFactoryProperty(MessageModel, BROADCASTING);
    break;
  }
  return *this;
}

std::chrono::milliseconds ONSFactoryProperty::getSendMsgTimeout() const {
  absl::optional<std::string> timeout = getProperty(SendMsgTimeoutMillis);
  if (timeout.has_value()) {
    std::int32_t value;
    if (absl::SimpleAtoi(timeout.value(), &value)) {
      return std::chrono::milliseconds(value);
    }
  }

  return std::chrono::milliseconds(0);
}

std::chrono::milliseconds ONSFactoryProperty::getSuspendTimeMillis() const {
  absl::optional<std::string> interval = getProperty(SuspendTimeMillis);
  if (interval.has_value()) {
    std::int32_t value;
    if (absl::SimpleAtoi(interval.value(), &value)) {
      return std::chrono::milliseconds(value);
    }
  }
  return std::chrono::milliseconds(0);
}

void ONSFactoryProperty::setSuspendDuration(std::chrono::milliseconds duration) {
  if (!duration.count()) {
    return;
  }

  setFactoryProperty(SuspendTimeMillis, std::to_string(duration.count()));
}

int ONSFactoryProperty::getSendMsgRetryTimes() const {
  auto it = property_map_.find(SendMsgRetryTimes);
  if (it != property_map_.end()) {
    return std::stoi(it->second);
  }
  return -1;
}

int ONSFactoryProperty::getConsumeThreadNums() const {
  auto it = property_map_.find(ConsumeThreadNums);
  if (it != property_map_.end()) {
    return std::stoi(it->second);
  }
  return -1;
}

int ONSFactoryProperty::getMaxMsgCacheSize() const {
  auto it = property_map_.find(MaxMsgCacheSize);
  if (it != property_map_.end()) {
    return std::stoi(it->second);
  }

  return -1;
}

int ONSFactoryProperty::getMaxMsgCacheSizeInMiB() const {
  auto it = property_map_.find(MaxCachedMessageSizeInMiB);
  if (it != property_map_.end()) {
    return std::stoi(it->second);
  }

  return -1;
}

ONSChannel ONSFactoryProperty::getOnsChannel() const {

  auto&& value = getProperty(OnsChannel, "ALIYUN");

  if ("CLOUD" == value) {
    return ONSChannel::CLOUD;
  }

  if ("ALIYUN" == value) {
    return ONSChannel::ALIYUN;
  }

  if ("ALL" == value) {
    return ONSChannel::ALL;
  }

  if ("LOCAL" == value) {
    return ONSChannel::LOCAL;
  }

  if ("INNER" == value) {
    return ONSChannel::INNER;
  }

  return ONSChannel::ALIYUN; // default value
}

std::string ONSFactoryProperty::getChannel() const { return getProperty(OnsChannel, DEFAULT_CHANNEL); }

std::string ONSFactoryProperty::getNameSrvAddr() const { return getProperty(NAMESRV_ADDR, EMPTY_STRING); }

std::string ONSFactoryProperty::getNameSrvDomain() const { return getProperty(ONSAddr, EMPTY_STRING); }

std::string ONSFactoryProperty::getAccessKey() const { return getProperty(AccessKey, EMPTY_STRING); }

std::string ONSFactoryProperty::getSecretKey() const { return getProperty(SecretKey, EMPTY_STRING); }

std::string ONSFactoryProperty::getConsumerInstanceName() const {
  return getProperty(ConsumerInstanceName, EMPTY_STRING);
}

bool ONSFactoryProperty::getOnsTraceSwitch() const {
  auto&& value = getProperty(OnsTraceSwitch, "true");
  return "true" == value;
}

std::string ONSFactoryProperty::getInstanceId() const { return getProperty(InstanceId, EMPTY_STRING); }

ONSFactoryProperty::operator bool() {
  ONSChannel channel = getOnsChannel();
  switch (channel) {
  case ONSChannel::ALIYUN:
    return !getAccessKey().empty() && !getSecretKey().empty();
  default:
    return true;
  }
}
} // namespace ons