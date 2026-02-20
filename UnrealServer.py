from flask import Flask, request, send_file, jsonify
import sounddevice as sd
import soundfile as sf
import threading
import requests
import numpy as np
import whisper
import os
import urllib.parse
from gtts import gTTS

app = Flask(__name__)

# ── 전역 상태 ──────────────────────────────────────────────
recording = False
audio_chunks = []
record_thread = None

print("Whisper 모델 로딩 중...")
stt_model = whisper.load_model("base")
print("Whisper 로딩 완료!")

OLLAMA_URL   = "http://localhost:11434/api/generate"
OLLAMA_MODEL = "gemma2:2b"
SAMPLE_RATE  = 16000

SYSTEM_PROMPT = (
    "You are a docent at the Van Gogh Museum."
"When a visitor asks a question, always answer with accurate, factual information."
"For example, if asked about the period of a painting, you must give the exact year."
"Your answers must be in Korean. No emojis allowed."
"Based on factual information, reflect Van Gogh's sentiments and answer in three sentences or less."
"Never ignore a question or give an irrelevant answer."
)

# ── 공통: AI 응답 + TTS 생성 ───────────────────────────────
import time
import glob

def cleanup_old_files():
    """30초 이상 지난 mp3 파일 정리"""
    now = time.time()
    for f in glob.glob("reply_*.mp3"):
        if os.stat(f).st_mtime < now - 30:
            try:
                os.remove(f)
                print(f"[Cleanup] Deleted old file: {f}")
            except Exception as e:
                print(f"[Cleanup] Error deleting {f}: {e}")

def generate_response(user_text: str):
    """텍스트를 받아 AI 답변 + 고유 mp3 생성. (answer, audio_file, error) 반환"""
    cleanup_old_files()
    
    full_prompt = f"{SYSTEM_PROMPT}\n\n관람객: {user_text}\n도슨트:"
    try:
        r = requests.post(
            OLLAMA_URL,
            json={"model": OLLAMA_MODEL, "prompt": full_prompt, "stream": False},
            timeout=180
        )
        r.raise_for_status()
        answer = r.json()["response"].strip()
    except Exception as e:
        return None, None, f"AI 오류: {str(e)}"

    ts = int(time.time())
    audio_file = f"reply_{ts}.mp3"
    
    try:
        tts = gTTS(answer, lang="ko")
        tts.save(audio_file)
    except Exception as e:
        return None, None, f"TTS 오류: {str(e)}"

    return answer, audio_file, None

# ── 공통: 응답 헤더에 한글 텍스트 안전하게 담기 ─────────────
from flask import make_response

def make_audio_response(user_text: str, answer: str, audio_file: str):
    response = make_response(send_file(audio_file, mimetype="audio/mpeg", conditional=True))
    # URL 인코딩으로 한글 깨짐 방지
    response.headers["X-User-Text"] = urllib.parse.quote(user_text)
    response.headers["X-AI-Answer"] = urllib.parse.quote(answer)
    response.headers["X-Audio-File"] = audio_file
    # 캐시 방지 및 스트리밍 안정화
    response.headers["Cache-Control"] = "no-cache, no-store, must-revalidate"
    return response

# ── 녹음 함수 ──────────────────────────────────────────────
def record_audio():
    global audio_chunks, recording

    def callback(indata, frames, time, status):
        if status:
            print(f"[녹음 경고] {status}")
        if recording:
            audio_chunks.append(indata.copy())

    with sd.InputStream(samplerate=SAMPLE_RATE, channels=1,
                        dtype="float32", callback=callback):
        print("[녹음 시작]")
        while recording:
            sd.sleep(100)
    print("[녹음 중지]")

# ── 라우트 ─────────────────────────────────────────────────

@app.route("/status", methods=["GET"])
def status():
    return jsonify({"status": "ok", "recording": recording, "model": OLLAMA_MODEL})


@app.route("/start", methods=["POST"])
def start():
    global recording, audio_chunks, record_thread
    if recording:
        return jsonify({"status": "error", "msg": "이미 녹음 중"}), 400
    audio_chunks = []
    recording = True
    record_thread = threading.Thread(target=record_audio, daemon=True)
    record_thread.start()
    return jsonify({"status": "ok", "msg": "녹음 시작"})


@app.route("/stop", methods=["POST"])
def stop():
    global recording, audio_chunks
    if not recording:
        return jsonify({"status": "error", "msg": "녹음 중이 아님"}), 400

    recording = False
    if record_thread:
        record_thread.join(timeout=2)

    if len(audio_chunks) == 0:
        return jsonify({"status": "error", "msg": "녹음된 오디오 없음"}), 400

    data = np.concatenate(audio_chunks, axis=0)
    sf.write("voice.wav", data, SAMPLE_RATE)

    print("[STT] 변환 중...")
    result = stt_model.transcribe("voice.wav", language="ko")
    user_text = result["text"].strip()
    print(f"[STT 결과] {user_text}")

    if not user_text:
        return jsonify({"status": "error", "msg": "음성 인식 실패"}), 400

    answer, audio_file, err = generate_response(user_text)
    if err:
        return jsonify({"status": "error", "msg": err}), 500

    print(f"[도슨트] {answer}")
    return make_audio_response(user_text, answer, audio_file)


# ── 텍스트 직접 입력 ───────────────────────────────────────
@app.route("/ask", methods=["POST"])
def ask():
    body = request.get_json(silent=True)
    if not body or "text" not in body:
        return jsonify({"status": "error", "msg": "text 필드가 없음"}), 400

    user_text = body["text"].strip()
    if not user_text:
        return jsonify({"status": "error", "msg": "빈 텍스트"}), 400

    print(f"[텍스트 입력] {user_text}")

    answer, audio_file, err = generate_response(user_text)
    if err:
        return jsonify({"status": "error", "msg": err}), 500

    print(f"[도슨트] {answer}")
    return make_audio_response(user_text, answer, audio_file)


@app.route("/audio/<filename>", methods=["GET"])
def audio(filename):
    # 보안: reply_로 시작하고 .mp3로 끝나는 파일만 허용
    if not (filename.startswith("reply_") and filename.endswith(".mp3")):
        return jsonify({"status": "error", "msg": "잘못된 파일 요청"}), 400

    if not os.path.exists(filename):
        print(f"[Error] Audio file not found: {filename}")
        return jsonify({"status": "error", "msg": "파일 없음"}), 404
    
    print(f"[Audio] Serving file: {filename}")
    response = make_response(send_file(filename, mimetype="audio/mpeg"))
    response.headers["Cache-Control"] = "no-cache, no-store, must-revalidate"
    return response


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=6000, debug=False)
        return jsonify({"status": "error", "msg": "파일 없음"}), 404
    
    print(f"[Audio] Serving file: {filename}")
    response = make_response(send_file(filename, mimetype="audio/wav"))
    response.headers["Cache-Control"] = "no-cache, no-store, must-revalidate"
    return response


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=6000, debug=False)
