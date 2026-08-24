#!/usr/bin/env python3
"""Multi-frame visual audit with Zhipu Vision (GLM-4.6V -> 4V-Plus -> 4V-Flash fallback)."""
import argparse, base64, json, os, sys, time
from pathlib import Path
from collections import Counter

API_KEY = os.environ.get("ZHIPU_API_KEY", "")
API_URL = "https://open.bigmodel.cn/api/paas/v4/chat/completions"
MODEL_CANDIDATES = ["glm-4.6v", "glm-4v-plus", "glm-4v-flash"]
ACTIVE_MODEL = None

def discover_model():
	if not API_KEY:
		raise RuntimeError("ZHIPU_API_KEY is not set.")
    for model in MODEL_CANDIDATES:
        try:
            import urllib.request
            payload = {"model": model, "messages": [{"role": "user", "content": "Say ok."}], "max_tokens": 5}
            headers = {"Authorization": "Bearer " + API_KEY, "Content-Type": "application/json"}
            req = urllib.request.Request(API_URL, data=json.dumps(payload).encode(), headers=headers)
            with urllib.request.urlopen(req, timeout=20) as resp:
                data = json.loads(resp.read().decode())
            if "choices" in data:
                print("  Model " + model + " available")
                return model
        except Exception as e:
            print("  Model " + model + " not available: " + str(e))
    raise RuntimeError("No Zhipu vision model available.")

def encode_image(path):
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode()

def build_multi_frame_prompt(filenames, frame_index, total_groups):
    names = ", ".join(filenames)
    return (
        "You are a visual auditor for a Chinese driving test simulation game (KeMuSanTraining). "
        "Here are " + str(len(filenames)) + " consecutive screenshots from the same test drive "
        "(group " + str(frame_index) + "/" + str(total_groups) + ": " + names + "). Analyze them together:\n\n"
        "1. Road surface color - asphalt should be dark grey, NOT pure white or black. Grass should be natural green.\n"
        "2. Lane markings - yellow center dashes, white edge lines, zebra crossings, stop lines visible?\n"
        "3. Buildings - city blocks on both sides with color variety? Does it feel like a city?\n"
        "4. Traffic - other vehicles visible? Moving? Oncoming/same-direction? Density reasonable?\n"
        "5. Pedestrians - any near crosswalks?\n"
        "6. Overall realism - does it look like a real city driving road?\n"
        "7. Frame consistency - do buildings/roads/markings stay consistent? Any flickering/clipping?\n\n"
        "Reply ONLY with JSON:\n"
        '{"score": 1-10, "road_tone": "...", "markings": "...", "buildings": "...", '
        '"traffic": "...", "pedestrians": "...", "realism": "...", '
        '"consistency": "...", "issues": ["..."]}'
    )

def call_api(messages, max_tokens=1200):
    global ACTIVE_MODEL
    if ACTIVE_MODEL is None:
        ACTIVE_MODEL = discover_model()
    payload = {"model": ACTIVE_MODEL, "messages": messages, "max_tokens": max_tokens, "temperature": 0.2}
    headers = {"Authorization": "Bearer " + API_KEY, "Content-Type": "application/json"}
    for attempt in range(3):
        try:
            import urllib.request
            req = urllib.request.Request(API_URL, data=json.dumps(payload).encode(), headers=headers)
            with urllib.request.urlopen(req, timeout=120) as resp:
                data = json.loads(resp.read().decode())
            content = data["choices"][0]["message"]["content"].strip()
            if content.startswith("```"):
                content = content.split("\n", 1)[1]
                if content.endswith("```"):
                    content = content.rsplit("```", 1)[0]
                content = content.strip()
            start = content.find("{")
            end = content.rfind("}")
            if start >= 0 and end > start:
                content = content[start:end+1]
            return json.loads(content)
        except json.JSONDecodeError as e:
            if attempt == 2:
                return {"error": "JSON parse failed", "score": 0}
            time.sleep(2)
        except Exception as e:
            if attempt == 2:
                idx = MODEL_CANDIDATES.index(ACTIVE_MODEL)
                if idx < len(MODEL_CANDIDATES) - 1:
                    ACTIVE_MODEL = MODEL_CANDIDATES[idx + 1]
                    print("  Falling back to " + ACTIVE_MODEL + "...")
                    return call_api(messages, max_tokens)
                return {"error": str(e), "score": 0}
            time.sleep(3)
    return {"error": "failed", "score": 0}

def review_single(image_path):
    b64 = encode_image(image_path)
    prompt = "Driving test game visual auditor. Evaluate: road color, markings, buildings, traffic, pedestrians, realism. Reply JSON: {\"score\":1-10, \"road_tone\":\"...\", \"markings\":\"...\", \"buildings\":\"...\", \"traffic\":\"...\", \"pedestrians\":\"...\", \"realism\":\"...\", \"issues\":[...]}"
    messages = [{"role": "user", "content": [{"type": "text", "text": prompt}, {"type": "image_url", "image_url": {"url": "data:image/png;base64," + b64}}]}]
    return call_api(messages, 600)

def review_group(image_paths, group_idx, total_groups):
    filenames = [os.path.basename(p) for p in image_paths]
    prompt = build_multi_frame_prompt(filenames, group_idx, total_groups)
    content = [{"type": "text", "text": prompt}]
    for p in image_paths:
        b64 = encode_image(p)
        content.append({"type": "image_url", "image_url": {"url": "data:image/png;base64," + b64}})
    messages = [{"role": "user", "content": content}]
    return call_api(messages, 1200)

def batch_review(directory, output_path=None, group_size=3):
    pngs = sorted(Path(directory).glob("*.png"))
    if not pngs:
        wedir = Path(directory) / "WindowsEditor"
        if wedir.is_dir():
            pngs = sorted(wedir.glob("*.png"))
    if not pngs:
        print("No PNG files found in " + directory)
        return []
    groups = [pngs[i:i+group_size] for i in range(0, len(pngs), group_size)]
    total_groups = len(groups)
    print("Found " + str(len(pngs)) + " screenshots -> " + str(total_groups) + " groups")

    results = []
    for gi, group in enumerate(groups):
        paths = [str(p) for p in group]
        names = " + ".join(p.name for p in group)
        print("\n[" + str(gi+1) + "/" + str(total_groups) + "] Reviewing: " + names, flush=True)
        if len(group) == 1:
            res = review_single(paths[0])
        else:
            res = review_group(paths, gi + 1, total_groups)
        res["_files"] = [p.name for p in group]
        res["_group"] = gi + 1
        results.append(res)
        score = res.get("score", "?")
        issues = res.get("issues", [])
        print("  Score: " + str(score) + "/10  |  Issues: " + str(len(issues)))
        for iss in issues[:3]:
            print("    - " + str(iss))
        time.sleep(1.5)

    scores = [r.get("score", 0) for r in results if isinstance(r.get("score"), (int, float))]
    avg = sum(scores) / len(scores) if scores else 0
    all_issues = []
    for r in results:
        all_issues.extend(r.get("issues", []))

    print("\n" + "="*60)
    print("SUMMARY")
    print("Groups: " + str(len(results)) + "  |  Screenshots: " + str(len(pngs)))
    print("Average score: " + str(round(avg, 1)) + "/10")
    print("Model: " + str(ACTIVE_MODEL))
    if all_issues:
        print("\nTop issues:")
        for issue, count in Counter(all_issues).most_common(10):
            print("  [" + str(count) + "x] " + str(issue))

    if output_path:
        with open(output_path, "w", encoding="utf-8") as f:
            f.write("# KeMuSanTraining Visual Audit Report\n\n")
            f.write("**Model**: " + str(ACTIVE_MODEL) + "  |  **Avg score**: " + str(round(avg, 1)) + "/10\n\n")
            f.write("| Grp | Files | Score | Road | Markings | Buildings | Traffic | Peds | Realism | Consistency |\n")
            f.write("|-----|-------|-------|------|----------|-----------|---------|------|---------|-------------|\n")
            for r in results:
                g = r.get("_group", "?")
                fs = ", ".join(r.get("_files", []))
                sc = r.get("score", "?")
                f.write("| " + str(g) + " | " + fs + " | " + str(sc) + " | " +
                    str(r.get("road_tone", "") or "")[:25] + " | " +
                    str(r.get("markings", "") or "")[:25] + " | " +
                    str(r.get("buildings", "") or "")[:25] + " | " +
                    str(r.get("traffic", "") or "")[:25] + " | " +
                    str(r.get("pedestrians", "") or "")[:25] + " | " +
                    str(r.get("realism", "") or "")[:25] + " | " +
                    str(r.get("consistency", "") or "")[:25] + " |\n")
            if all_issues:
                f.write("\n## Top Issues (" + str(len(all_issues)) + " total)\n\n")
                for issue, count in Counter(all_issues).most_common(20):
                    f.write("- [" + str(count) + "x] " + str(issue) + "\n")
            f.write("\n---\n*Generated by visual_review.py using " + str(ACTIVE_MODEL) + "*\n")
        print("\nReport written to " + output_path)
    return results

def main():
    parser = argparse.ArgumentParser(description="Multi-frame visual review with Zhipu Vision")
    parser.add_argument("--single", type=str)
    parser.add_argument("--batch", type=str)
    parser.add_argument("--output", type=str, default=None)
    parser.add_argument("--group-size", type=int, default=3)
    args = parser.parse_args()
    if not args.single and not args.batch:
        parser.error("Must specify --single or --batch")
    if args.single:
        if not os.path.isfile(args.single):
            print("File not found: " + args.single)
            sys.exit(1)
        res = review_single(args.single)
        print(json.dumps(res, indent=2, ensure_ascii=False))
    if args.batch:
        batch_review(args.batch, args.output, args.group_size)

if __name__ == "__main__":
    main()
