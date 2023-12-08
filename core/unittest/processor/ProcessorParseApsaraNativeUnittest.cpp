// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdlib>

#include "common/JsonUtil.h"
#include "config/Config.h"
#include "models/LogEvent.h"
#include "plugin/instance/ProcessorInstance.h"
#include "processor/ProcessorParseApsaraNative.h"
#include "processor/ProcessorSplitLogStringNative.h"
#include "processor/ProcessorSplitRegexNative.h"
#include "unittest/Unittest.h"

namespace logtail {

class ProcessorParseApsaraNativeUnittest : public ::testing::Test {
public:
    void SetUp() override {
        mContext.SetConfigName("project##config_0");
        BOOL_FLAG(ilogtail_discard_old_data) = false;
    }

    void TestInit();
    void TestProcessWholeLine();
    void TestProcessWholeLinePart();
    void TestProcessKeyOverwritten();
    void TestUploadRawLog();
    void TestAddLog();
    void TestProcessEventKeepUnmatch();
    void TestProcessEventDiscardUnmatch();
    void TestMultipleLines();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessWholeLine);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessWholeLinePart);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessKeyOverwritten);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestUploadRawLog);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestAddLog);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessEventKeepUnmatch);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessEventDiscardUnmatch);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestMultipleLines);

void ProcessorParseApsaraNativeUnittest::TestMultipleLines() {
    // 第一个contents 测试多行下的解析，第二个contents测试多行下time的解析
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:50.1]\t[ERROR]\t[1]\t/ilogtail/AppConfigBase.cpp:1\t\tAppConfigBase AppConfigBase:1
[2023-09-04 13:15:33.2]\t[INFO]\t[2]\t/ilogtail/AppConfigBase.cpp:2\t\tAppConfigBase AppConfigBase:2
[2023-09-04 13:15:22.3]\t[WARNING]\t[3]\t/ilogtail/AppConfigBase.cpp:3\t\tAppConfigBase AppConfigBase:3",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15
:50.1]\t[ERROR]\t[1]\t/ilogtail/AppConfigBase.cpp:1\t\tAppConfigBase AppConfigBase:1
[2023-09-04 13:15:22.3]\t[WARNING]\t[3]\t/ilogtail/AppConfigBase.cpp:3\t\tAppConfigBase AppConfigBase:3",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";


    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "1",
                    "AppConfigBase AppConfigBase": "1",
                    "__LEVEL__": "ERROR",
                    "__THREAD__": "1",
                    "log.file.offset": "0",
                    "microtime": "1693833350100000"
                },
                "timestamp": 1693833350,
                "timestampNanosecond": 100000000,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "2",
                    "AppConfigBase AppConfigBase": "2",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "2",
                    "log.file.offset": "0",
                    "microtime": "1693833333200000"
                },
                "timestamp": 1693833333,
                "timestampNanosecond": 200000000,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "3",
                    "AppConfigBase AppConfigBase": "3",
                    "__LEVEL__": "WARNING",
                    "__THREAD__": "3",
                    "log.file.offset": "0",
                    "microtime": "1693833322300000"
                },
                "timestamp": 1693833322,
                "timestampNanosecond": 300000000,
                "type": 1
            },
            {
                "contents": {
                    "__raw__": "[2023-09-04 13:15",
                    "log.file.offset": "0"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            },
            {
                "contents": {
                    "__raw__": ":50.1]\t[ERROR]\t[1]\t/ilogtail/AppConfigBase.cpp:1\t\tAppConfigBase AppConfigBase:1",
                    "log.file.offset": "0"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "3",
                    "AppConfigBase AppConfigBase": "3",
                    "__LEVEL__": "WARNING",
                    "__THREAD__": "3",
                    "log.file.offset": "0",
                    "microtime": "1693833322300000"
                },
                "timestamp": 1693833322,
                "timestampNanosecond": 300000000,
                "type": 1
            }
        ]
    })";

    // ProcessorSplitLogStringNative
    {
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        eventGroup.FromJsonString(inJson);

        // make config
        Json::Value config;
        config["SourceKey"] = "content";
        config["Timezone"] = "GMT+00:00";
        config["KeepingSourceWhenParseFail"] = true;
        config["KeepingSourceWhenParseSucceed"] = false;
        config["CopingRawLog"] = false;
        config["RenamedSourceKey"] = "__raw__";
        config["AppendingLogPositionMeta"] = false;

        std::string pluginId = "testID";
        // run function ProcessorSplitLogStringNative
        ProcessorSplitLogStringNative processorSplitLogStringNative;
        processorSplitLogStringNative.SetContext(mContext);
        APSARA_TEST_TRUE_FATAL(processorSplitLogStringNative.Init(config));
        processorSplitLogStringNative.Process(eventGroup);

        // run function ProcessorParseApsaraNative
        ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
        ProcessorInstance processorInstance(&processor, pluginId);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
        processor.Process(eventGroup);

        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // ProcessorSplitRegexNative
    {
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        eventGroup.FromJsonString(inJson);

        // make config
        Json::Value config;
        config["SourceKey"] = "content";
        config["Timezone"] = "GMT+00:00";
        config["KeepingSourceWhenParseFail"] = true;
        config["KeepingSourceWhenParseSucceed"] = false;
        config["CopingRawLog"] = false;
        config["RenamedSourceKey"] = "__raw__";
        config["StartPattern"] = ".*";
        config["UnmatchedContentTreatment"] = "split";
        config["AppendingLogPositionMeta"] = false;

        std::string pluginId = "testID";

        // run function ProcessorSplitRegexNative
        ProcessorSplitRegexNative processorSplitRegexNative;
        processorSplitRegexNative.SetContext(mContext);
        APSARA_TEST_TRUE_FATAL(processorSplitRegexNative.Init(config));
        processorSplitRegexNative.Process(eventGroup);

        // run function ProcessorParseApsaraNative
        ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
        ProcessorInstance processorInstance(&processor, pluginId);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
        processor.Process(eventGroup);

        // judge result
        std::string outJson = eventGroup.ToJsonString();

        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorParseApsaraNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";
    config["Timezone"] = "";

    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
}

void ProcessorParseApsaraNativeUnittest::TestProcessWholeLine() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";
    config["Timezone"] = "";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:04.862181]	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:16:04.862181]	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[1693833364862181]	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "__file_offset__": "0",
                    "microtime": "1693833304862181"
                },
                "timestamp": 1693833304,
                "timestampNanosecond": 862181000,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "__file_offset__": "0",
                    "microtime": "1693833364862181"
                },
                "timestamp": 1693833364,
                "timestampNanosecond": 862181000,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "__file_offset__": "0",
                    "microtime": "1693833364862181"
                },
                "timestamp": 1693833364,
                "timestampNanosecond": 862181000,
                "type": 1
            }
        ]
    })";
    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseApsaraNativeUnittest::TestProcessWholeLinePart() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = false;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";
    config["Timezone"] = "";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:0]	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:16:0[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[1234560	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    // check observablity
    int count = 3;
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().parseFailures);
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcInRecordsTotal->GetValue());
    // discard unmatch, so output is 0
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processorInstance.mProcOutRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processor.mProcParseOutSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcDiscardRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcParseErrorTotal->GetValue());
}

void ProcessorParseApsaraNativeUnittest::TestProcessKeyOverwritten() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = true;
    config["CopingRawLog"] = true;
    config["RenamedSourceKey"] = "rawLog";
    config["Timezone"] = "";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:04.862181]	[INFO]	[385658]	content:100		rawLog:success		__raw_log__:success",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "__file_offset__": "0",
                    "__raw_log__": "success",
                    "content": "100",
                    "microtime": "1693833304862181",
                    "rawLog": "success"
                },
                "timestamp": 1693833304,
                "timestampNanosecond": 862181000,
                "type": 1
            },
            {
                "contents" :
                {
                    "__file_offset__": "0",
                    "__raw_log__": "value1",
                    "rawLog": "value1"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            }
        ]
    })";
    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseApsaraNativeUnittest::TestUploadRawLog() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = true;
    config["CopingRawLog"] = true;
    config["RenamedSourceKey"] = "rawLog";
    config["Timezone"] = "";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:04.862181]	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "__file_offset__": "0",
                    "microtime": "1693833304862181",
                    "rawLog" : "[2023-09-04 13:15:04.862181]\t[INFO]\t[385658]\t/ilogtail/AppConfigBase.cpp:100\t\tAppConfigBase AppConfigBase:success"
                },
                "timestamp": 1693833304,
                "timestampNanosecond": 862181000,
                "type": 1
            },
            {
                "contents" :
                {
                    "__file_offset__": "0",
                    "__raw_log__": "value1",
                    "rawLog": "value1"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            }
        ]
    })";
    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseApsaraNativeUnittest::TestAddLog() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";

    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    auto logEvent = LogEvent::CreateEvent(sourceBuffer);
    char key[] = "key";
    char value[] = "value";
    processor.AddLog(key, value, *logEvent);
    // check observability
    APSARA_TEST_EQUAL_FATAL(int(strlen(key) + strlen(value) + 5),
                            processor.GetContext().GetProcessProfile().logGroupSize);
}

void ProcessorParseApsaraNativeUnittest::TestProcessEventKeepUnmatch() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value1",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // judge result
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "__file_offset__": "0",
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__file_offset__": "0",
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__file_offset__": "0",
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__file_offset__": "0",
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__file_offset__": "0",
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    // check observablity
    int count = 5;
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().parseFailures);
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue = "value1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseInSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcOutRecordsTotal->GetValue());
    expectValue = "rawLogvalue1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseOutSizeBytes->GetValue());

    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processor.mProcDiscardRecordsTotal->GetValue());

    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcParseErrorTotal->GetValue());
}

void ProcessorParseApsaraNativeUnittest::TestProcessEventDiscardUnmatch() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = false;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value1",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    // check observablity
    int count = 5;
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().parseFailures);
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue = "value1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseInSizeBytes->GetValue());
    // discard unmatch, so output is 0
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processorInstance.mProcOutRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processor.mProcParseOutSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcDiscardRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcParseErrorTotal->GetValue());
}

} // namespace logtail

UNIT_TEST_MAIN