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
            print(answer['content'])


if __name__ == "__main__":

    print(
        "Type anything then press enter. "
        "Type exit or quit to do so."
    )

    main()
