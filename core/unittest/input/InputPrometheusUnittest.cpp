// Copyright 2024 iLogtail Authors
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

#include <curl/curl.h>
#include <json/json.h>

#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <filesystem>
#include <memory>
#include <string>

#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "input/InputPrometheus.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineContext.h"
#include "prometheus/PrometheusInputRunner.h"
#include "queue/ProcessQueueManager.h"
#include "unittest/Unittest.h"

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
using tcp = net::ip::tcp; // from <boost/asio/ip/tcp.hpp>

using namespace std;
namespace logtail {

// MockHttpClient
class MockHttpClient : public sdk::HTTPClient {
public:
    MockHttpClient();

    virtual void Send(const std::string& httpMethod,
                      const std::string& host,
                      const int32_t port,
                      const std::string& url,
                      const std::string& queryString,
                      const std::map<std::string, std::string>& header,
                      const std::string& body,
                      const int32_t timeout,
                      sdk::HttpMessage& httpMessage,
                      const std::string& intf,
                      const bool httpsFlag);
    virtual void AsynSend(sdk::AsynRequest* request);
    
    bool mDoScrape = false;
};

MockHttpClient::MockHttpClient() {
}

void MockHttpClient::Send(const std::string& httpMethod,
                          const std::string& host,
                          const int32_t port,
                          const std::string& url,
                          const std::string& queryString,
                          const std::map<std::string, std::string>& header,
                          const std::string& body,
                          const int32_t timeout,
                          sdk::HttpMessage& httpMessage,
                          const std::string& intf,
                          const bool httpsFlag) {
    std::cout << "httpMethod=" << httpMethod << "\n"
              << "host=" << host << "\n"
              << "port=" << port << "\n"
              << "url=" << url << "\n"
              << "queryString=" << queryString << "\n"
              << "timeout=" << timeout << "\n"
              << "httpsFlag=" << httpsFlag << "\n"
              << std::endl;
    mDoScrape = true;
    httpMessage.content
        = "# HELP go_gc_duration_seconds A summary of the pause duration of garbage collection cycles.\n"
          "# TYPE go_gc_duration_seconds summary\n"
          "go_gc_duration_seconds{quantile=\"0\"} 1.5531e-05\n"
          "go_gc_duration_seconds{quantile=\"0.25\"} 3.9357e-05\n"
          "go_gc_duration_seconds{quantile=\"0.5\"} 4.1114e-05\n"
          "go_gc_duration_seconds{quantile=\"0.75\"} 4.3372e-05\n"
          "go_gc_duration_seconds{quantile=\"1\"} 0.000112326\n"
          "go_gc_duration_seconds_sum 0.034885631\n"
          "go_gc_duration_seconds_count 850\n"
          "# HELP go_goroutines Number of goroutines that currently exist.\n"
          "# TYPE go_goroutines gauge\n"
          "go_goroutines 7\n"
          "# HELP go_info Information about the Go environment.\n"
          "# TYPE go_info gauge\n"
          "go_info{version=\"go1.22.3\"} 1\n"
          "# HELP go_memstats_alloc_bytes Number of bytes allocated and still in use.\n"
          "# TYPE go_memstats_alloc_bytes gauge\n"
          "go_memstats_alloc_bytes 6.742688e+06\n"
          "# HELP go_memstats_alloc_bytes_total Total number of bytes allocated, even if freed.\n"
          "# TYPE go_memstats_alloc_bytes_total counter\n"
          "go_memstats_alloc_bytes_total 1.5159292e+08";
    httpMessage.statusCode = 200;
}

void MockHttpClient::AsynSend(sdk::AsynRequest* request) {
}
class InputPrometheusUnittest : public testing::Test {
public:
    void OnSuccessfulInit();
    void OnPipelineUpdate();
    void TestScrapeData();

protected:
    static void SetUpTestCase() { AppConfig::GetInstance()->mPurageContainerMode = true; }
    void SetUp() override {
        p.mName = "test_config";
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(p);

        setenv("POD_NAME", "matrix-test", 1);
        setenv("OPERATOR_HOST", "127.0.0.1", 1);
        setenv("OPERATOR_PORT", "12345", 1);
        if (StringToJsonValue(mConfigString, mConfig)) {
            std::cout << "JSON parsing succeeded." << std::endl;
            std::cout << mConfig.toStyledString() << std::endl;
        } else {
            std::cerr << "JSON parsing failed." << std::endl;
        }
    }
    void TearDown() override {
        unsetenv("POD_NAME");
        unsetenv("OPERATOR_HOST");
        unsetenv("OPERATOR_PORT");
    }
    bool StringToJsonValue(const std::string& jsonString, Json::Value& jsonValue) {
        Json::CharReaderBuilder readerBuilder;
        std::string errs;
        std::istringstream iss(jsonString);
        bool parsingSuccessful = Json::parseFromStream(readerBuilder, iss, &jsonValue, &errs);
        if (!parsingSuccessful) {
            std::cerr << "Failed to parse JSON: " << errs << std::endl;
        }
        return parsingSuccessful;
    }
    uint32_t mPluginIndex = 0;
    Json::Value mConfig;
    std::string mConfigString = R"JSON(
{
    "Type": "input_prometheus",
    "ScrapeConfig": {
        "enable_http2": true,
        "follow_redirects": true,
        "honor_timestamps": false,
        "job_name": "_kube-state-metrics",
        "kubernetes_sd_configs": [
            {
                "enable_http2": true,
                "follow_redirects": true,
                "kubeconfig_file": "",
                "namespaces": {
                    "names": [
                        "arms-prom"
                    ],
                    "own_namespace": false
                },
                "role": "pod"
            }
        ],
        "metrics_path": "/metrics",
        "relabel_configs": [
            {
                "action": "keep",
                "regex": "kube-state-metrics",
                "replacement": "$1",
                "separator": ";",
                "source_labels": [
                    "__meta_kubernetes_pod_label_k8s_app"
                ]
            },
            {
                "action": "keep",
                "regex": "8080",
                "replacement": "$1",
                "separator": ";",
                "source_labels": [
                    "__meta_kubernetes_pod_container_port_number"
                ]
            },
            {
                "action": "replace",
                "regex": "([^:]+)(?::\\d+)?;(\\d+)",
                "replacement": "$1:$2",
                "separator": ";",
                "source_labels": [
                    "__address__",
                    "__meta_kubernetes_pod_container_port_number"
                ],
                "target_label": "__address__"
            }
        ],
        "scheme": "http",
        "scrape_interval": "3s",
        "scrape_timeout": "3s"
    }
}
    )JSON";

private:
    Pipeline p;
    PipelineContext ctx;
};

void InputPrometheusUnittest::OnSuccessfulInit() {
    unique_ptr<InputPrometheus> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    // only mandatory param
    configStr = R"(
        {
            "Type": "input_prometheus",
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputPrometheus());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputPrometheus::sName, "1");
    APSARA_TEST_FALSE(input->Init(configJson, mPluginIndex, optionalGoPipeline));

    // with scrape job
    configStr = R"(
        {
            "Type": "input_prometheus",
            "ScrapeConfig": {
                "job_name": "_arms-prom/node-exporter/0",
                "metrics_path": "/metrics",
                "scheme": "http",
                "scrape_interval": "15s",
                "scrape_timeout": "15s",
                "scrape_targets": [
                    {
                        "host": "172.17.0.3:9100",
                    }
                ]
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputPrometheus());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputPrometheus::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, mPluginIndex, optionalGoPipeline));
    APSARA_TEST_EQUAL("_arms-prom/node-exporter/0", input->mScrapeJobPtr->mJobName);
    APSARA_TEST_EQUAL("/metrics", input->mScrapeJobPtr->mMetricsPath);
    APSARA_TEST_EQUAL("15s", input->mScrapeJobPtr->mScrapeIntervalString);
    APSARA_TEST_EQUAL("15s", input->mScrapeJobPtr->mScrapeTimeoutString);

    PrometheusInputRunner::GetInstance()->Stop();
}

void InputPrometheusUnittest::OnPipelineUpdate() {
    unique_ptr<InputPrometheus> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    configStr = R"(
        {
            "Type": "input_prometheus",
            "ScrapeConfig": {
                "job_name": "_arms-prom/node-exporter/0",
                "metrics_path": "/metrics",
                "scheme": "http",
                "scrape_interval": "15s",
                "scrape_timeout": "15s",
                "scrape_targets": [
                    {
                        "host": "172.17.0.3:9100",
                    }
                ]
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputPrometheus());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputPrometheus::sName, "1");

    APSARA_TEST_TRUE(input->Init(configJson, mPluginIndex, optionalGoPipeline));

    APSARA_TEST_TRUE(input->Start());
    APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->mPrometheusInputsMap.find("test_config")
                     != PrometheusInputRunner::GetInstance()->mPrometheusInputsMap.end());

    APSARA_TEST_TRUE(input->Stop(true));
    APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->mPrometheusInputsMap.find("test_config")
                     == PrometheusInputRunner::GetInstance()->mPrometheusInputsMap.end());

    PrometheusInputRunner::GetInstance()->Stop();
}

http::response<http::string_body> handle_request(const http::request<http::string_body>& req,
                                                 const std::unordered_map<std::string, std::string>& response_map) {
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/plain");
    res.keep_alive(req.keep_alive());
    auto it = response_map.find(std::string(req.target()));
    if (it != response_map.end()) {
        res.body() = it->second;
    } else {
        res.result(http::status::not_found);
        res.body() = "Resource not found";
    }
    res.prepare_payload();
    return res;
}

void do_session(tcp::socket socket, const std::unordered_map<std::string, std::string>& response_map) {
    try {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req);
        http::response<http::string_body> res = handle_request(req, response_map);
        http::write(socket, res);
        socket.shutdown(tcp::socket::shutdown_send);
    } catch (std::exception const& e) {
        std::cerr << "Error in session: " << e.what() << std::endl;
    }
}

void start_operator_server() {
    try {
        auto const address = net::ip::make_address(getenv("OPERATOR_HOST"));
        unsigned short port = static_cast<unsigned short>(std::stoi(getenv("OPERATOR_PORT")));
        std::cout << "Server starting at " << address.to_string() << ":" << port << std::endl;
        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {address, port}};
        std::unordered_map<std::string, std::string> response_map = {
            {"/jobs/_kube-state-metrics/targets?collector_id=matrix-test", R"JSON([
    {
        "targets": [
            "192.168.22.7:8080"
        ],
        "labels": {
            "__meta_kubernetes_pod_controller_kind": "ReplicaSet",
            "__meta_kubernetes_pod_container_image": "registry-vpc.cn-hangzhou.aliyuncs.com/acs/kube-state-metrics:v2.3.0-a71f78c-aliyun",
            "__meta_kubernetes_namespace": "arms-prom",
            "__meta_kubernetes_pod_labelpresent_pod_template_hash": "true",
            "__meta_kubernetes_pod_uid": "00d1897f-d442-47c4-8423-e9bf32dea173",
            "__meta_kubernetes_pod_container_init": "false",
            "__meta_kubernetes_pod_container_port_protocol": "TCP",
            "__meta_kubernetes_pod_host_ip": "192.168.21.234",
            "__meta_kubernetes_pod_controller_name": "kube-state-metrics-64cf88c8f4",
            "__meta_kubernetes_pod_annotation_k8s_aliyun_com_pod_ips": "192.168.22.7",
            "__meta_kubernetes_pod_ready": "true",
            "__meta_kubernetes_pod_node_name": "cn-hangzhou.192.168.21.234",
            "__meta_kubernetes_pod_annotationpresent_k8s_aliyun_com_pod_ips": "true",
            "__address__": "192.168.22.7:8080",
            "__meta_kubernetes_pod_labelpresent_k8s_app": "true",
            "__meta_kubernetes_pod_label_k8s_app": "kube-state-metrics",
            "__meta_kubernetes_pod_container_id": "containerd://57c4dfd8d9ea021defb248dfbc5cc3bd3758072c4529be351b8cc6838bdff02f",
            "__meta_kubernetes_pod_container_port_number": "8080",
            "__meta_kubernetes_pod_ip": "192.168.22.7",
            "__meta_kubernetes_pod_phase": "Running",
            "__meta_kubernetes_pod_container_name": "kube-state-metrics",
            "__meta_kubernetes_pod_container_port_name": "http-metrics",
            "__meta_kubernetes_pod_label_pod_template_hash": "64cf88c8f4",
            "__meta_kubernetes_pod_name": "kube-state-metrics-64cf88c8f4-jtn6v"
        }
    },
    {
        "targets": [
            "192.168.22.31:6443"
        ],
        "labels": {
            "__address__": "192.168.22.31:6443",
            "__meta_kubernetes_endpoint_port_protocol": "TCP",
            "__meta_kubernetes_service_label_provider": "kubernetes",
            "__meta_kubernetes_endpoints_name": "kubernetes",
            "__meta_kubernetes_service_name": "kubernetes",
            "__meta_kubernetes_endpoints_labelpresent_endpointslice_kubernetes_io_skip_mirror": "true",
            "__meta_kubernetes_service_labelpresent_provider": "true",
            "__meta_kubernetes_endpoint_port_name": "https",
            "__meta_kubernetes_namespace": "default",
            "__meta_kubernetes_service_label_component": "apiserver",
            "__meta_kubernetes_service_labelpresent_component": "true",
            "__meta_kubernetes_endpoint_ready": "true"
        }
    }
])JSON"},
        };
        for (;;) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);
            std::thread{[&response_map](tcp::socket socket) { do_session(std::move(socket), response_map); },
                        std::move(socket)}
                .detach();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void InputPrometheusUnittest::TestScrapeData() {
    std::thread server_thread([]() { start_operator_server(); });
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ProcessQueueManager::GetInstance()->CreateOrUpdateQueue(ctx.GetProcessQueueKey(), 0);

    MockHttpClient* client = new MockHttpClient();

    PrometheusInputRunner::GetInstance()->Start();

    unique_ptr<InputPrometheus> input;
    Json::Value optionalGoPipeline;
    input.reset(new InputPrometheus());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputPrometheus::sName, "1");
    APSARA_TEST_TRUE(input->Init(mConfig, mPluginIndex, optionalGoPipeline));
    input->Start();

    std::this_thread::sleep_for(std::chrono::seconds(6));
    const auto& copy = ScraperGroup::GetInstance()->mScrapeJobMap["_kube-state-metrics"]->GetScrapeTargetsMapCopy();
    EXPECT_EQ(copy.size(), 1u);
    std::string hash;
    for (const auto& pair : copy) {
        EXPECT_TRUE(pair.first.find("192.168.22.7") != std::string::npos);
        EXPECT_EQ(pair.second->mTargets[0], "192.168.22.7:8080");
        EXPECT_EQ(pair.second->mLabels.size(), 6u);
        EXPECT_EQ(pair.second->mJobName, "_kube-state-metrics");
        EXPECT_EQ(pair.second->mMetricsPath, "/metrics");
        EXPECT_EQ(pair.second->mScheme, "http");
        EXPECT_EQ(pair.second->mHost, "192.168.22.7");
        EXPECT_EQ(pair.second->mPort, 8080);
        EXPECT_EQ(pair.second->mScrapeInterval, 3);
        EXPECT_EQ(pair.second->mScrapeTimeout, 3);
        hash = pair.second->mHash;
    }

    EXPECT_NE(nullptr, ScraperGroup::GetInstance()->mScrapeWorkMap["_kube-state-metrics"][hash]);
    ScraperGroup::GetInstance()->mScrapeWorkMap["_kube-state-metrics"][hash]->StopScrapeLoop();
    EXPECT_FALSE(client->mDoScrape);
    ScraperGroup::GetInstance()->mScrapeWorkMap["_kube-state-metrics"][hash]->mClient.reset(client);
    ScraperGroup::GetInstance()->mScrapeWorkMap["_kube-state-metrics"][hash]->StartScrapeLoop();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    EXPECT_TRUE(client->mDoScrape);

    PrometheusInputRunner::GetInstance()->Stop();
    server_thread.detach();
}

UNIT_TEST_CASE(InputPrometheusUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputPrometheusUnittest, OnPipelineUpdate)
UNIT_TEST_CASE(InputPrometheusUnittest, TestScrapeData)

} // namespace logtail

UNIT_TEST_MAIN