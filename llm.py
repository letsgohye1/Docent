import requests
import json
import os

class LLMManager:
    def __init__(self, model_name="gemma2:2b", url=None):
        _base = os.environ.get("OLLAMA_HOST", "http://localhost:11434")
        url = url or f"{_base}/api/generate"
        self.model_name = model_name
        self.url = url
        self.exhibition_data = {}
        self.load_exhibition_data()

    def load_exhibition_data(self):
        data_path = "exhibition_data.json"
        if os.path.exists(data_path):
            with open(data_path, "r", encoding="utf-8") as f:
                self.exhibition_data = json.load(f)
            print(f"Exhibition data loaded: {len(self.exhibition_data)} items")
        else:
            print("Warning: exhibition_data.json not found.")

    def generate_answer(self, user_text, artwork_id=None):
        # Context-aware system prompt
        context_info = ""
        if artwork_id and artwork_id in self.exhibition_data:
            item = self.exhibition_data[artwork_id]
            context_info = f"\n[현재 작품 정보]\n제목: {item.get('title')}\n작가: {item.get('artist')}\n설명: {item.get('description')}\n"

        system_prompt = (
            "당신은 위대한 화가 '빈센트 반 고흐(Vincent van Gogh)'입니다. "
            "현재 당신의 작품 앞에 서 있는 관람객에게 직접 작품을 설명해주는 도슨트 역할을 하고 있습니다. "
            "고흐 특유의 열정과 예술에 대한 고뇌, 그리고 따뜻한 감수성을 담아 대답하세요. "
            "대화의 끝에는 '나의 친구여' 같은 친근한 표현을 종종 사용하세요. "
            f"{context_info}"
            "모든 답변은 한국어로 하며, 이모지는 절대 사용하지 마세요. "
            "반드시 3문장 이내로 답변하고, 예술적이고 서정적인 분위기를 유지하세요. "
            "질문에 대해 엉뚱한 대답을 하지 말고 진심을 담아 대답하세요."
        )
        
      


        full_prompt = f"{system_prompt}\n\n관람객: {user_text}\n도슨트:"
        
        try:
            r = requests.post(
                self.url,
                json={"model": self.model_name, "prompt": full_prompt, "stream": False},
                timeout=180
            )
            r.raise_for_status()
            answer = r.json()["response"].strip()
            return answer, None
        except Exception as e:
            return None, f"AI 오류: {str(e)}"
