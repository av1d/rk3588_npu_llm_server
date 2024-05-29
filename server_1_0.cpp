#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <cpprest/http_listener.h>
#include <cpprest/http_msg.h>
#include <thread>
#include <string.h> /* rkllm includes begin here */
#include <unistd.h>
#include <string>
#include "rkllm.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <csignal>
#include <vector> /* end rkllm includes */

#define RKLLM_VERSION "1.0"

/* For models converted with RKLLM 1.0 only.
 * Use newer version for 1.0.1
 */

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;
using namespace utility;
using namespace std;

std::string PROMPT_TEXT_PREFIX;
std::string PROMPT_TEXT_POSTFIX;
std::string g_llmResponse;

LLMHandle llmHandle = nullptr;

void exit_handler(int signal)
{
    if (llmHandle != nullptr)
    {
        std::cout << "Caught exit signal. Exiting..." << std::endl;
        LLMHandle _tmp = llmHandle;
        llmHandle = nullptr;
        rkllm_destroy(_tmp);
        exit(signal);
    }
}

void callback(const char *text, void *userdata, LLMCallState state) {
    std::string* output = static_cast<std::string*>(userdata);
    if (state == LLM_RUN_FINISH) {
        printf("\n");
    } else if (state == LLM_RUN_ERROR) {
        printf("\nLLM run error\n");
    } else {
        *output += text;
    }
}

// parse the incoming JSON payload
void parse_json(const std::string& json_str, std::string& prompt_text_prefix, std::string& input_str, std::string& prompt_text_postfix) {
    try {
        boost::property_tree::ptree pt;
        std::stringstream ss(json_str);
        boost::property_tree::read_json(ss, pt);

        prompt_text_prefix = pt.get<std::string>("PROMPT_TEXT_PREFIX");
        input_str = pt.get<std::string>("input_str");
        prompt_text_postfix = pt.get<std::string>("PROMPT_TEXT_POSTFIX");
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    }
}

void handle_post(http_request message) {
    auto remote_address = message.remote_address();

    auto body = message.extract_string().get();
    std::string prompt_text_prefix, input_str, prompt_text_postfix;

    // Print the incoming IP and JSON request
    std::cout << remote_address << " - " << body << std::endl;

    parse_json(body, prompt_text_prefix, input_str, prompt_text_postfix);

    // Create JSON response with the parsed values
    json::value response_json;
    response_json[U("prompt_text_prefix")] = json::value::string(utility::conversions::to_string_t(prompt_text_prefix));
    response_json[U("input_str")] = json::value::string(utility::conversions::to_string_t(input_str));
    response_json[U("prompt_text_postfix")] = json::value::string(utility::conversions::to_string_t(prompt_text_postfix));

    std::string text = response_json[U("prompt_text_prefix")].as_string() +
                       response_json[U("input_str")].as_string() +
                       response_json[U("prompt_text_postfix")].as_string();

    std::string llmOutput;
    // Use the callback function to capture the LLM output
    int runResult = rkllm_run(llmHandle, text.c_str(), &llmOutput);

    if (runResult != 0) {
        printf("Error running LLM. Exiting...\n");
        return;
    }

    // Create a new JSON response with the LLM output
    json::value output_json;
    output_json[U("content")] = json::value::string(utility::conversions::to_string_t(llmOutput));

    // Prepare HTTP response
    http_response response(status_codes::OK);
    response.set_body(output_json.serialize());
    response.headers().add(U("Content-Type"), U("application/json"));

    message.reply(response).wait();
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <IP> <port> <rkllm_model_path>" << std::endl;
        return 1;
    }
    std::string rkllm_model_path = argv[3];

    signal(SIGINT, exit_handler);
    std::string rkllm_model(argv[3]);
    printf("Using RKLLM version %s\n", RKLLM_VERSION);
    printf("rkllm init start\n");

    RKLLMParam param = rkllm_createDefaultParam();
    param.modelPath = rkllm_model.c_str();
    param.target_platform = "rk3588";
    param.num_npu_core = 2;
    param.top_k = 1;
    param.max_new_tokens = 256;
    param.max_context_len = 512;

    int initResult = rkllm_init(&llmHandle, param, callback);
    if (initResult != 0) {
        printf("RKLLM init failed!\n");
        return -1;
    }
    printf("RKLLM init success!\n");


    std::string ip = argv[1];
    std::string port = argv[2];
    std::string url = "http://" + ip + ":" + port + "/";

    http_listener listener(U(url));

    listener.support(methods::POST, handle_post);

    try {
        listener.open().wait();
        std::cout << "\nUsing RKLLM model from: " << rkllm_model_path << std::endl;
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
