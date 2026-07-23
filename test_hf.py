import os
import json
import urllib.request
import urllib.error
from dotenv import load_dotenv

load_dotenv()
token = os.environ.get("HF_TOKEN", "")

if not token:
    print("HF_TOKEN is empty or not loaded.")
else:
    print(f"Token loaded, starts with: {token[:8]}...")

url = "https://router.huggingface.co/v1/chat/completions"
headers = {
    "Authorization": f"Bearer {token}",
    "Content-Type": "application/json"
}

system_instructions = (
    "You are a financial entity extraction AI. Analyze the user request and output exactly "
    "a valid Python dictionary with these exact keys: 'initial_investment' (float), "
    "'annual_costs' (list of floats matching lifespan), 'annual_benefits' (list of floats matching lifespan), "
    "'discount_rate' (float), 'project_lifespan' (int). "
    "Do not return any conversational text, explanations, or code blocks. Just the dictionary."
)

user_prompt = "Evaluate upgrading fleet infrastructure for $120k over a 5 year lifespan with 6k yearly costs and 40k yearly benefits at a 5% discount rate."

payload = {
    "model": "deepseek-ai/DeepSeek-V3-0324:fastest",
    "messages": [
        {"role": "system", "content": system_instructions},
        {"role": "user", "content": user_prompt}
    ],
    "temperature": 0.1
}

data = json.dumps(payload).encode("utf-8")
req = urllib.request.Request(url, data=data, headers=headers)

try:
    with urllib.request.urlopen(req, timeout=15) as response:
        print(response.read().decode("utf-8"))
except urllib.error.HTTPError as e:
    print(f"Status: {e.code}")
    print("Response body:")
    print(e.read().decode("utf-8"))