# =====================================================
# AI Docent Server - Dockerfile
# Stack: Python 3.11 + Whisper + Coqui XTTS v2 + Flask
# =====================================================
FROM python:3.11-slim

# System dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg \
    libsndfile1 \
    libportaudio2 \
    portaudio19-dev \
    gcc \
    g++ \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Install Python dependencies first (layer cache optimization)
COPY requirements.txt .
RUN pip install --no-cache-dir --upgrade pip && \
    pip install --no-cache-dir -r requirements.txt

# Copy application source files
COPY server.py stt.py llm.py tts.py exhibition_data.json ./

# Create directories
RUN mkdir -p tts_cache

# Expose Flask port
EXPOSE 6000

# Entrypoint
COPY docker-entrypoint.sh /docker-entrypoint.sh
RUN chmod +x /docker-entrypoint.sh

ENTRYPOINT ["/docker-entrypoint.sh"]
