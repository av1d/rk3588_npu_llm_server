#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cpprest/http_listener.h>
#include <cpprest/http_msg.h>
#include <thread>
#include <unistd.h>
#include "rkllm.h"
#include <fstream>
#include <sstream>
#include <csignal>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;
using namespace utility;
using namespace std;

std::string PROMPT_TEXT_PREFIX;
std::string INPUT_STR;
std::string PROMPT_TEXT_POSTFIX;
std::string g_llmResponse;
LLMHandle llmHandle = nullptr;

void exitHandler(int signal) {
    if (llmHandle != nullptr) {
        std::cout << "Caught exit signal. Exiting..." << std::endl;
        LLMHandle _tmp = llmHandle;
        llmHandle = nullptr;
        rkllm_destroy(_tmp);
        exit(signal);
    }
}

void callback(RKLLMResult *result, void *userdata, LLMCallState state) {
    std::string* output = static_cast<std::string*>(userdata);
    if (state == LLM_RUN_FINISH) {
        printf("\n");
    } else if (state == LLM_RUN_ERROR) {
        printf("\nLLM run error\n");
    } else {
        *output += result->text;
    }
}

void parseJson(const std::string& jsonStr, std::string& promptTextPrefix, std::string& inputStr, std::string& promptTextPostfix) {
    try {
        boost::property_tree::ptree pt;
        std::stringstream ss(jsonStr);
        boost::property_tree::read_json(ss, pt);

        promptTextPrefix = pt.get<std::string>("PROMPT_TEXT_PREFIX");
        inputStr = pt.get<std::string>("input_str");
        promptTextPostfix = pt.get<std::string>("PROMPT_TEXT_POSTFIX");
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    }
}

void handlePost(http_request message) {
    auto remoteAddress = message.remote_address();
    auto body = message.extract_string().get();
    std::string promptTextPrefix, inputStr, promptTextPostfix;

    std::cout << remoteAddress << " - " << body << std::endl;

    parseJson(body, promptTextPrefix, inputStr, promptTextPostfix);

    json::value responseJson;
    responseJson[U("prompt_text_prefix")] = json::value::string(utility::conversions::to_string_t(promptTextPrefix));
    responseJson[U("input_str")] = json::value::string(utility::conversions::to_string_t(inputStr));
    responseJson[U("prompt_text_postfix")] = json::value::string(utility::conversions::to_string_t(promptTextPostfix));

    std::string text = responseJson[U("prompt_text_prefix")].as_string() +
                       responseJson[U("input_str")].as_string() +
                       responseJson[U("prompt_text_postfix")].as_string();

    std::string llmOutput;
    int runResult = rkllm_run(llmHandle, text.c_str(), &llmOutput);

    if (runResult != 0) {
        std::cerr << "Error running LLM." << std::endl;
        json::value errorJson;
        errorJson[U("content")] = json::value::string(utility::conversions::to_string_t("Error running LLM."));

        http_response response(status_codes::InternalError);
        response.set_body(errorJson.serialize());
        response.headers().add(U("Content-Type"), U("application/json"));

        message.reply(response).wait();
        return;
    }

    json::value outputJson;
    outputJson[U("content")] = json::value::string(utility::conversions::to_string_t(llmOutput));

    http_response response(status_codes::OK);
    response.set_body(outputJson.serialize());
    response.headers().add(U("Content-Type"), U("application/json"));

    message.reply(response).wait();
}

void handleGet(http_request message) {
    json::value responseJson;
    responseJson[U("content")] = json::value::string(utility::conversions::to_string_t("online"));

    http_response response(status_codes::OK);
    response.set_body(responseJson.serialize());
    response.headers().add(U("Content-Type"), U("application/json"));

    message.reply(response).wait();
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <IP> <port> <rkllm_model_path>" << std::endl;
        return 1;
    }

    std::string rkllmModelPath = argv[3];
    signal(SIGINT, exitHandler);
    signal(SIGTERM, exitHandler);

    std::cout << "rkllm init start" << std::endl;

    RKLLMParam param = rkllm_createDefaultParam();
    param.model_path = rkllmModelPath.c_str();
    param.num_npu_core = 2;
    param.top_k = 1;
    param.max_new_tokens = 256;
    param.max_context_len = 512;

    int initResult = rkllm_init(&llmHandle, param, callback);
    if (initResult != 0) {
        std::cerr << "RKLLM init failed!" << std::endl;
        return -1;
    }

    std::cout << "RKLLM init success!" << std::endl;

    std::string ip = argv[1];
    std::string port = argv[2];
    std::string url = "http://" + ip + ":" + port + "/";

    http_listener listener(U(url));

    listener.support(methods::POST, handlePost);
    listener.support(methods::GET, handleGet);

    try {
        listener.open().wait();
        std::cout << "\nUsing RKLLM model from: " << rkllmModelPath << std::endl;
        std::cout << "Do not use in a production deployment." << std::endl;
        std::cout << "Listening for HTTP requests on " << ip << ":" << port << "..." << std::endl;

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in HTTP server: " << e.what() << std::endl;
    }

    rkllm_destroy(llmHandle);

    return 0;
}
