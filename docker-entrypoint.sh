#!/bin/bash
set -e

echo "==================================="
echo " AI Docent Server - Starting Up"
echo "==================================="

OLLAMA_HOST_URL="${OLLAMA_HOST:-http://ollama:11434}"
MODEL="gemma2:2b"

# Wait for Ollama to be ready
echo "[Entrypoint] Waiting for Ollama at ${OLLAMA_HOST_URL}..."
for i in $(seq 1 30); do
    if curl -s "${OLLAMA_HOST_URL}/api/tags" > /dev/null 2>&1; then
        echo "[Entrypoint] Ollama is ready!"
        break
    fi
    echo "[Entrypoint] Attempt ${i}/30 - Ollama not ready yet, waiting 5s..."
    sleep 5
done

# Pull the model if not already present
echo "[Entrypoint] Checking for model: ${MODEL}"
if curl -s "${OLLAMA_HOST_URL}/api/tags" | grep -q "${MODEL}"; then
    echo "[Entrypoint] Model '${MODEL}' already exists, skipping pull."
else
    echo "[Entrypoint] Pulling model '${MODEL}'... (this may take a while)"
    curl -s -X POST "${OLLAMA_HOST_URL}/api/pull" \
        -H "Content-Type: application/json" \
        -d "{\"name\": \"${MODEL}\"}" | tail -1
    echo "[Entrypoint] Model pull complete."
fi

echo "[Entrypoint] Starting Flask server on port 6000..."
exec python server.py
