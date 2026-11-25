"""Read public metadata for the existing CI run."""
from html import unescape
import re
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

REPO = "challanger2000/125A-Chopperator"
RUN = "45506553532"
SOURCE = "a276f0228acf58230ee5ca5c48267f062b5c79c1"
BASE = f"https://github.com/{REPO}/actions/runs/{RUN}"
JOBS = {"portable": "138843140206", "windows": "138843229177"}
ARTIFACTS = {
    "14973327568": "b1663051d81942950cd44a7f4bab940beddd728e0957d0a3f4456927f16d1191",
    "14972703147": "7bc18e6325249a27cdc3e0505723835da5c50ad816a1a993ee7a92211d4b0665",
    "14971514705": "9c88228c2e8492c1708a0a720d8d2af113cd607f858a22e17e254b9068b05752",
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
    for step, number, conclusion in steps:
        print(f"  {number} {unescape(step)}: {conclusion}")
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
