"""Verify the public GitHub Actions record, without inferring downloaded bytes."""
from html import unescape
import re
from urllib.request import Request, urlopen

BASE = "https://github.com/challanger2000/125A-Chopperator/actions/runs/45487606108"
JOB = "138777034387"
SOURCE = "aade95612ccf492819221b16ff896d11d138c907"
DIGESTS = {
    "14963589373": "93940bf719053c2095117c87927394dfbfe743ca62c3f485e93b677c57d96776",
    "14963229366": "ce1b985febb4067b980416b403fb652344907e996ee5663c7f80ca342571452a",
    "14963873935": "436c798444834cfe3d5473a334e703ea1b51bde058e1d9894556974e39c03c7e",
}


def get(url):
    request = Request(url, headers={"User-Agent": "P009 audit metadata check"})
    with urlopen(request, timeout=20) as response:
        return response.read().decode("utf-8")


run = get(BASE)
job = get(f"{BASE}/job/{JOB}")
assert f"/commit/{SOURCE}" in run
assert re.search(r'class="h4 color-fg-default">Success</span>', run)
assert f'data-job-status="completed"' in job
print(f"GitHub Actions run 45487606108 job {JOB} commit {SOURCE} status Success")
steps = re.findall(r'data-name="([^"]+)"\s+data-number="(\d+)"\s+data-conclusion="([^"]+)"', job)
assert steps and all(conclusion == "success" for _, _, conclusion in steps)
for name, number, conclusion in steps:
    print(f"step {number} {unescape(name)}: {conclusion}")
for artifact, expected in DIGESTS.items():
    found = re.search(r'id="artifact-'+artifact+r'-hash"[^>]*>\s*sha256:([a-f0-9]{64})', run)
    assert found and found[1] == expected, artifact
    print(f"published artifact {artifact} sha256:{found[1]}")
print("Public result and metadata only. Individual job logs and artifact bytes not included.")
