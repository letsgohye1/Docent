import os
import time
import glob
import hashlib
import shutil
import numpy as np
import soundfile as sf
import torch
from TTS.api import TTS

class TTSManager:
    def __init__(self, cache_dir="tts_cache"):
        print("Coqui TTS (XTTS v2) 로딩 중...")
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        self.tts = TTS("tts_models/multilingual/multi-dataset/xtts_v2", progress_bar=False)
        self.tts.to(self.device)
        print(f"Coqui TTS 로딩 완료! (Device: {self.device})")
        
        self.cache_dir = cache_dir
        if not os.path.exists(self.cache_dir):
            os.makedirs(self.cache_dir)

    def cleanup_old_files(self):
        now = time.time()
        for f in glob.glob("reply_*.wav"):
            if os.stat(f).st_mtime < now - 600:
                try:
                    os.remove(f)
                except:
                    pass

    def split_text(self, text, max_len=200):
        import re
        # Clean text: remove characters that might cause issues for TTS
        # Keep basic punctuation: . , ! ? and Korean/English/Numbers
        text = re.sub(r'[^가-힣a-zA-Z0-9\s.,!?]', ' ', text)
        text = re.sub(r'\s+', ' ', text).strip()

        # Split by sentence markers
        sentences = re.split(r'([.!?])', text)
        result = []
        current = ""
        
        for i in range(0, len(sentences), 2):
            s = sentences[i]
            p = sentences[i+1] if i+1 < len(sentences) else ""
            combined = (s + p).strip()
            if not combined: continue
            
            # If a single sentence is too long, split it by space
            if len(combined) > max_len:
                if current:
                    result.append(current.strip())
                    current = ""
                # Hard split long sentence
                words = combined.split(' ')
                temp_s = ""
                for w in words:
                    if len(temp_s) + len(w) < max_len:
                        temp_s += " " + w
                    else:
                        result.append(temp_s.strip())
                        temp_s = w
                current = temp_s
            elif len(current) + len(combined) < max_len:
                current += " " + combined
            else:
                result.append(current.strip())
                current = combined
                
        if current: result.append(current.strip())
        
        # Final filter: remove very short or empty results
        result = [s for s in result if len(s) > 1]
        return result[:30] # Limit to 30 chunks to prevent infinite loops/timeout

    def generate_audio(self, text, speaker_wav="docent_voice.wav"):
        if not text or not text.strip():
            return None, "TTS 오류: 제공된 텍스트가 비어있습니다."

        self.cleanup_old_files()
        
        text_hash = hashlib.md5(text.encode('utf-8')).hexdigest()
        cache_path = os.path.join(self.cache_dir, f"{text_hash}.wav")
        
        ts = int(time.time())
        output_file = f"reply_{ts}.wav"

        if os.path.exists(cache_path):
            shutil.copy(cache_path, output_file)
            return output_file, None

        if not os.path.exists(speaker_wav):
            return None, f"TTS 오류: '{speaker_wav}' 파일이 없습니다."

        try:
            sentences = self.split_text(text)
            if not sentences:
                return None, "TTS 오류: 유효한 문장이 없습니다."

            combined_data = []
            samplerate = 24000
            
            for i, sentence in enumerate(sentences):
                temp_wav = f"temp_{ts}_{i}.wav"
                try:
                    print(f"[TTS] Synthesizing {i+1}/{len(sentences)}: {sentence[:30]}...")
                    self.tts.tts_to_file(
                        text=sentence,
                        speaker_wav=speaker_wav,
                        language="ko",
                        file_path=temp_wav
                    )
                    
                    if os.path.exists(temp_wav):
                        data, rate = sf.read(temp_wav)
                        samplerate = rate
                        combined_data.append(data)
                        os.remove(temp_wav)
                    else:
                        print(f"[TTS Warning] Temp file not created for segment {i}")
                except Exception as segment_err:
                    print(f"[TTS Segment Error] index {i}: {segment_err}")
                    continue

            if not combined_data:
                return None, "TTS 오류: 모든 문장 합성에 실패했습니다."

            final_data = np.concatenate(combined_data)
            
            # Normalize volume
            max_val = np.abs(final_data).max()
            if max_val > 0:
                final_data = (final_data / max_val) * 0.9
                
            sf.write(output_file, final_data, samplerate, subtype='PCM_16')
            shutil.copy(output_file, cache_path)
            return output_file, None
            
        except Exception as e:
            import traceback
            traceback.print_exc()
            return None, f"TTS 합성 오류: {str(e)}"
