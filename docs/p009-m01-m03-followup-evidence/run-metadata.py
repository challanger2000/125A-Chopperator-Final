"""Read public metadata for the existing CI run."""
from html import unescape
import re
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

REPO = "challanger2000/125A-Chopperator"
RUN = "45512723609"
SOURCE = "8859e8be9a3aa12a9f5439780d369e273a820e79"
BASE = f"https://github.com/{REPO}/actions/runs/{RUN}"
JOBS = {"portable": "138864267703", "windows": "138864417915"}
ARTIFACTS = {
    "14977440471": "1b04d0dcff486506e649edf9d26baf68c83a1b36b1b41d936364ba66d5d45f70",
    "14977420599": "f9c68f25de96abf643ee795b9e1e38a3dbcb57a1b1ed334d9bcff4e8e93644c5",
    "14977116333": "526bb76d1afba6d40651e7f0572113d312b140884bac040e125c2a9989293964",
}


def request(url):
    return urlopen(Request(url, headers={"User-Agent": "P009 independent M01-M03 audit"}), timeout=20)


def page(url):
    with request(url) as result:
        return result.read().decode()


def status(url):
    try:
        with request(url) as result:
            return f"HTTP {result.status}"
    except HTTPError as error:
        return f"HTTP {error.code}"
    except URLError as error:
        return str(error.reason)


run = page(BASE)
assert f"/commit/{SOURCE}" in run
assert re.search(r'class="h4 color-fg-default">Success</span>',run)
print(f"Run {RUN} commit {SOURCE}: Success")

for name, job in JOBS.items():
    assert f"/job/{job}" in run
    data = page(f"{BASE}/job/{job}")
    assert 'data-job-status="completed"' in data
    steps = re.findall(r'data-name="([^"]+)"\s+data-number="(\d+)"\s+data-conclusion="([^"]+)"', data)
    assert steps and all(conclusion == "success" for _, _, conclusion in steps)
    print(f"Job {name} {job}: {len(steps)} successful steps")
    if name == "windows":
        editor = [(unescape(step), number, result) for step, number, result in steps
                  if unescape(step) == "Native editor interaction and resize"]
        assert editor == [("Native editor interaction and resize", "9", "success")]
        print("  Step 9 Native editor interaction and resize: success")
    if "Sign in to view logs" in data:
        print("  Public page requires sign-in to view logs")

for artifact, expected in ARTIFACTS.items():
    found = re.search(r'id="artifact-'+artifact+r'-hash"[^>]*>\s*sha256:([a-f0-9]{64})',run)
    assert found and found[1] == expected, artifact
    print(f"Artifact {artifact} published sha256:{found[1]}")
    print(f"Artifact {artifact} download: {status(f'{BASE}/artifacts/{artifact}')}")

print("Windows editor step log:",
      status(f"https://github.com/{REPO}/commit/{SOURCE}/checks/{JOBS['windows']}/logs/9"))
print("Actions logs API:",
      status(f"https://api.github.com/repos/{REPO}/actions/jobs/{JOBS['windows']}/logs"))
