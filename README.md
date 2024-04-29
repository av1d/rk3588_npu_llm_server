# Server/Web API for RK3588 NPU LLM

The goal is to make LLMs running on the NPU practical and usable as I'm not a fan of the CLI interactions due to their limited usability. The server outputs a JSON response and therefore you can use cURL, AJAX, Python, or whatever you want.  

Currently [only works with Qwen](https://huggingface.co/Pelochus/qwen-1_8B-rk3588).  

First, install ezrknpu from Pelochus if you haven't yet:  
[https://github.com/Pelochus/ezrknpu](https://github.com/Pelochus/ezrknpu)  
Side note: Parts of this server are borrowed from the original `ezrknn-llm/rkllm-runtime/example/src/main.cpp` file from that repo.

Next, install Boost:  
`sudo apt install libboost-all-dev libcpprest-dev`

Test Boost. Put this in a file named test.cpp:
```
#include <iostream>
#include <boost/version.hpp>

int main() {
    std::cout << "Using Boost " << BOOST_VERSION / 100000 << "."  // major version
              << BOOST_VERSION / 100 % 1000 << "."  // minor version
              << BOOST_VERSION % 100  // patch level
              << std::endl;
    return 0;
}
```

Compile: `g++ test.cpp`
Run test: `./a.out`
Result should give you something like "`Using Boost 1.74.0`"  
If not, RTFM: https://www.boost.org/doc/libs/1_74_0/  

Compile server.cpp. Change the path to rkllmrt if needed.  
If you have `locate` installed, try `locate rkllmrt`.  
The path in the following command is probably correct, though:  
`g++ server.cpp -o server -std=c++11 -lcpprest -lcrypto -L/usr/lib -lrkllmrt`

Finally, you have a file named `server` in the current working directory.  
Syntax is: IP, port, path to model. Start it:  
`./server 192.168.0.196 31337 ../qwen-1_8B-rk3588/qwen-chat-1_8B.rkllm`

Test it. Change the value of "hello, how are you " if you like then send it.  
`curl -H "Content-Type: application/json" -d '{"PROMPT_TEXT_PREFIX":"<|im_start|>system You are a helpful assistant. <|im_end|> <|im_start|>user ","input_str":"hello, how are you ","PROMPT_TEXT_POSTFIX":"<|im_end|><|im_start|>assistant "}' http://192.168.0.196:31337/`

Implement it in Python:  
```
import requests
import sys

IP_ADDR = '192.168.0.196'
PORT = '31337'


def ask_ai(user_input: str) -> dict:

    headers = {
        'Content-Type': 'application/json',
    }

    prefix = (
        "<|im_start|>system You are a helpful assistant. <|im_end|> "
        "<|im_start|>user "
    )

    postfix = (
        "<|im_end|><|im_start|>assistant "
    )

    json_data = {
        'PROMPT_TEXT_PREFIX': prefix,
        'input_str': str(user_input) + ' ',
        'PROMPT_TEXT_POSTFIX': postfix,
    }

    response = requests.post(
        f"http://{IP_ADDR}:{PORT}",
        headers=headers,
        json=json_data
    )

    return response.json()

def main():

    while True:
        user_input = input("ai> ")
        if user_input.lower() in ['quit', 'exit']:
            sys.exit("Goodbye!")
        else:
            answer = ask_ai(user_input)
            print(answer['output'])


if __name__ == "__main__":

    print(
        "Type anything then press enter. "
        "Type exit or quit to do so."
    )

    main()
```

implement it in PHP:  
```
<?php
$ch = curl_init();
curl_setopt($ch, CURLOPT_URL, 'http://192.168.0.196:31337/');
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ch, CURLOPT_CUSTOMREQUEST, 'POST');
curl_setopt($ch, CURLOPT_HTTPHEADER, [
    'Content-Type: application/json',
]);
curl_setopt($ch, CURLOPT_POSTFIELDS, '{"PROMPT_TEXT_PREFIX":"<|im_start|>system You are a helpful assistant. <|im_end|> <|im_start|>user ","input_str":"hello, how are you ","PROMPT_TEXT_POSTFIX":"<|im_end|><|im_start|>assistant "}');

$response = curl_exec($ch);

curl_close($ch);

var_dump($response);
```

jQuery:  
```
$.ajax({
  url: 'http://192.168.0.196:31337/',
  crossDomain: true,
  method: 'post',
  contentType: 'application/json',
  data: JSON.stringify({
    'PROMPT_TEXT_PREFIX': '<|im_start|>system You are a helpful assistant. <|im_end|> <|im_start|>user ',
    'input_str': 'hello, how are you ',
    'PROMPT_TEXT_POSTFIX': '<|im_end|><|im_start|>assistant '
  })
}).done(function(response) {
  console.log(response);
});
```

Parts of this software taken from `ezrknn-llm/rkllm-runtime/example/src/main.cpp` are covered by the Apache License:
```
Copyright (c) 2024 by Rockchip Electronics Co., Ltd. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

Everything else falls under the MIT license.  

Do not run this server in a production environment. It is lacking sanitization and security features.  
Shouts to [r/RockchipNPU](https://old.reddit.com/r/RockchipNPU/), check it out!
