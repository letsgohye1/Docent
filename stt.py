import whisper
import os

class STTManager:
    def __init__(self, model_size="base"):
        print(f"Whisper 모델 로딩 중 ({model_size})...")
        self.model = whisper.load_model(model_size)
        print("Whisper 로딩 완료!")

    def transcribe(self, audio_data, language="ko"):
        """
        audio_data can be a file path (string) or a numpy array (float32).
        Passing a numpy array bypasses the need for ffmpeg to probe the file.
        """
        try:
            # If audio_data is a string, check if file exists
            if isinstance(audio_data, str):
                if not os.path.exists(audio_data):
                    return None, "오디오 파일이 없습니다."
            
            # Whisper's transcribe method can accept a numpy array directly
            result = self.model.transcribe(audio_data, language=language)
            text = result["text"].strip()
            return text, None
        except Exception as e:
            return None, f"STT 오류: {str(e)}"
