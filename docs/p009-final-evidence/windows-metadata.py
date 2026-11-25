"""Read the existing public Windows run record. This script starts no jobs."""
from html import unescape
import re
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

RUN = "45502305140"
BASE = "https://github.com/challanger2000/125A-Chopperator/actions/runs/" + RUN
JOB = "138828442951"
SOURCE = "3bea0580c10fb30943c405b230bdf3268519c6e2"
ARTIFACTS = {
    "14972535704": ("125A-Chopperator-P009-QA-evidence",
                   "ac13219e6302c4d5d44516b317e885b471c09f73ce043674abd73045a5112f57"),
    "14971293433": ("125A-Chopperator-P009-Windows-x64-development",
                   "2889cbd9c6aed33ec08438508bd367a08e0e282cbb6143339bf5f52a39a22b1a"),
    "14971084787": ("125A-Chopperator-P009-verified-candidate-archive",
                   "3d0b02294ecc2acf329623f39af9aac39e4a62f855af8bbb4445431be6cee80f"),
}


def request(url):
    return urlopen(Request(url, headers={"User-Agent": "P009 audit metadata check"}), timeout=20)


def html(url):
    with request(url) as response:
        return response.read().decode("utf-8")


def status(url):
    try:
        with request(url) as response:
            return f"HTTP {response.status}"
    except HTTPError as exc:
        return f"HTTP {exc.code}"
    except URLError as exc:
        return str(exc.reason)


run = html(BASE)
job = html(f"{BASE}/job/{JOB}")
assert f"/commit/{SOURCE}" in run
assert re.search(r'class="h4 color-fg-default">Success</span>', run)
assert 'data-job-status="completed"' in job
print(f"GitHub Actions run {RUN} job {JOB} commit {SOURCE} status Success")
steps = re.findall(r'data-name="([^"]+)"\s+data-number="(\d+)"\s+data-conclusion="([^"]+)"', job)
assert steps and all(conclusion == "success" for _, _, conclusion in steps)
for name, number, conclusion in steps:
    print(f"step {number} {unescape(name)}: {conclusion}")
for artifact, (name, expected) in ARTIFACTS.items():
    row = re.search(r'data-artifact-id="' + artifact + r'">(.*?)</tr>', run, re.S)
    assert row and name in row[1], artifact
    found = re.search(r'id="artifact-' + artifact + r'-hash"[^>]*>\s*sha256:([a-f0-9]{64})', row[1])
    assert found and found[1] == expected, artifact
    print(f"published artifact {artifact} {name} sha256:{found[1]}")
    print(f"artifact access {artifact}: {status(f'{BASE}/artifacts/{artifact}')}")
if re.search(r"Sign in to view logs", job):
    print("Public job page: Sign in to view logs")
api = f"https://api.github.com/repos/challanger2000/125A-Chopperator/actions/runs/{RUN}"
print(f"API access: {status(api)}")
logs = f"https://github.com/challanger2000/125A-Chopperator/commit/{SOURCE}/checks/{JOB}/logs/14"
print(f"step 14 log access: {status(logs)}")
print("Verified public result and artifact metadata. Raw job logs and artifact bytes were not retained.")
