from flask import Flask, request, send_file, jsonify, make_response
import os
import time
import urllib.parse
import numpy as np
import sounddevice as sd
import soundfile as sf
import threading

from stt import STTManager
from llm import LLMManager
from tts import TTSManager

app = Flask(__name__)

# 전역 상태 및 매니저 초기화
recording = False
audio_chunks = []
record_thread = None
SAMPLE_RATE = 16000

stt = STTManager()
llm = LLMManager()
tts = TTSManager()

def make_audio_response(user_text, answer, audio_file):
    response = make_response(send_file(audio_file, mimetype="audio/wav", conditional=True))
    response.headers["X-User-Text"] = urllib.parse.quote(user_text)
    response.headers["X-AI-Answer"] = urllib.parse.quote(answer)
    response.headers["X-Audio-File"] = audio_file
    response.headers["Cache-Control"] = "no-cache, no-store, must-revalidate"
    return response

@app.route("/status", methods=["GET"])
def status():
    return jsonify({"status": "ok", "recording": recording})

@app.route("/start", methods=["POST"])
def start():
    global recording, audio_chunks, record_thread
    if recording: return jsonify({"status": "error", "msg": "이미 녹음 중"}), 400
    audio_chunks = []
    recording = True
    
    def record_audio():
        global recording
        try:
            def callback(indata, frames, time, status):
                if recording: audio_chunks.append(indata.copy())
            with sd.InputStream(samplerate=SAMPLE_RATE, channels=1, dtype="float32", callback=callback):
                while recording: sd.sleep(100)
        except Exception as e:
            print(f"[Critical Audio Error] {e}")
            print("마이크를 찾을 수 없거나 초기화에 실패했습니다. 기본 녹음 장치를 확인하세요.")
            recording = False

    record_thread = threading.Thread(target=record_audio, daemon=True)
    record_thread.start()
    return jsonify({"status": "ok", "msg": "녹음 시작"})

@app.route("/stop", methods=["POST"])
def stop():
    global recording, audio_chunks
    if not recording: return jsonify({"status": "error", "msg": "Recording is not in progress."}), 400
    
    try:
        recording = False
        if record_thread: record_thread.join(timeout=2)
        if not audio_chunks: return jsonify({"status": "error", "msg": "No audio data recorded."}), 400

        print(f"[Server] Processing {len(audio_chunks)} audio chunks...")
        data = np.concatenate(audio_chunks, axis=0).flatten()
        # Save for debugging
        sf.write("voice.wav", data, SAMPLE_RATE)

        # 1. STT
        user_text, err = stt.transcribe(data)
        if err: 
            print(f"[STT Error] {err}")
            return jsonify({"status": "error", "msg": f"STT error: {err}"}), 500
        
        # Preprocessing & Validation
        user_text = user_text.strip()
        print(f"[User Voice] '{user_text}'")
        
        if not user_text:
            print("[Server] User text is empty, skipping AI response.")
            return jsonify({"status": "empty", "msg": "No speech detected."}), 200

        # Truncate extremely long input
        if len(user_text) > 1000:
            user_text = user_text[:1000]

        # 2. LLM
        artwork_id = request.args.get("artwork_id")
        answer, err = llm.generate_answer(user_text, artwork_id)
        if err: 
            print(f"[LLM Error] {err}")
            return jsonify({"status": "error", "msg": f"LLM error: {err}"}), 500

        if not answer or not answer.strip():
            return jsonify({"status": "error", "msg": "AI generated an empty response."}), 500

        # 3. TTS
        audio_file, err = tts.generate_audio(answer)
        if err:
            import traceback
            print(f"[TTS Error] Synthesis failed: {err}")
            traceback.print_exc()
            return jsonify({"status": "error", "msg": f"TTS error: {err}"}), 500

        # 4. Final Verification
        if not os.path.exists(audio_file):
            return jsonify({"status": "error", "msg": "Audio file not generated."}), 500

        print(f"[Server] Successfully processed voice question. Audio: {audio_file}")
        return make_audio_response(user_text, answer, audio_file)

    except Exception as e:
        import traceback
        print(f"[Critical Server Error] {e}")
        traceback.print_exc()
        return jsonify({"status": "error", "msg": f"Unexpected server error: {str(e)}"}), 500

@app.route("/ask", methods=["POST"])
def ask():
    import traceback, os, re

    body = request.get_json(silent=True) or {}
    user_text = body.get("text", "").strip()
    artwork_id = body.get("artwork_id")

    if not user_text:
        return jsonify({"status": "error", "msg": "No text provided."}), 400

    # Input Preprocessing: Remove suspicious special characters
    user_text = re.sub(r'[^가-힣a-zA-Z0-9\s.,!?]', '', user_text)

    try:
        print(f"[User Text] '{user_text}' (Artwork: {artwork_id})")
        
        # 1. LLM
        answer, err = llm.generate_answer(user_text, artwork_id)
        if err:
            print(f"[LLM Error] {err}")
            return jsonify({"status": "error", "msg": f"LLM error: {err}"}), 500
        
        if not answer or not answer.strip():
            return jsonify({"status": "error", "msg": "AI generated an empty response."}), 500

        print(f"[LLM Success] Answer: {answer[:50]}...")

        # 2. TTS
        audio_file, err = tts.generate_audio(answer)
        if err:
            print(f"[TTS Error] {err}")
            traceback.print_exc()
            return jsonify({"status": "error", "msg": f"TTS error: {err}"}), 500

        if not os.path.exists(audio_file):
            return jsonify({"status": "error", "msg": "Audio file not created."}), 500

        # 3. Success
        return make_audio_response(user_text, answer, audio_file)

    except Exception as e:
        print(f"[Unexpected Server Error] {e}")
        traceback.print_exc()
        return jsonify({"status": "error", "msg": f"Unexpected server error: {str(e)}"}), 500

@app.route("/audio/<filename>", methods=["GET"])
def get_audio(filename):
    if not os.path.exists(filename): return jsonify({"status": "error", "msg": "파일 없음"}), 404
    return send_file(filename, mimetype="audio/wav")

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=6000, debug=False)
