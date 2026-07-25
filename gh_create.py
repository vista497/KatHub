#!/usr/bin/env python3
import subprocess, json, urllib.request, sys

# Step 1: get token from git credential fill
cred = "protocol=https\nhost=github.com\n\n"
result = subprocess.run(['git', 'credential', 'fill'], input=cred, capture_output=True, text=True)
password = None
username = None
for line in result.stdout.split('\n'):
    if line.startswith('password='):
        password = line.split('=', 1)[1]
    elif line.startswith('username='):
        username = line.split('=', 1)[1]

if not password:
    print("ERROR: could not get token from git credential fill", file=sys.stderr)
    sys.exit(1)

token = password
print(f"User: {username}, token: {token[:4]}...{token[-4:]}")

# Step 2: check if repo exists
def api(method, url, data=None):
    headers = {
        'Authorization': f'token {token}',
        'Accept': 'application/vnd.github+json',
        'User-Agent': 'hermes'
    }
    body = json.dumps(data).encode() if data else None
    req = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        resp = urllib.request.urlopen(req)
        return json.loads(resp.read()), resp.status
    except urllib.error.HTTPError as e:
        return json.loads(e.read()), e.code

data, status = api('GET', 'https://api.github.com/repos/vista497/KatHub')

if status == 404:
    print("Repo does not exist, creating...")
    data, status = api('POST', 'https://api.github.com/user/repos', {
        'name': 'KatHub',
        'description': 'AI Controller — плагинная архитектура, Qt 6.7 + Vue 3, Tailscale VPN',
        'private': False,
        'has_issues': True,
        'has_wiki': False
    })
    if status == 201:
        print(f"CREATED: {data['html_url']}")
    else:
        print(f"FAILED ({status}): {data.get('message', 'unknown error')}")
        sys.exit(1)
else:
    print(f"EXISTS: {data['html_url']}")
