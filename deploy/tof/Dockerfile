# syntax=docker/dockerfile:1
ARG PYTHON_BASE=python:3.11-slim-bookworm
FROM $PYTHON_BASE
ARG SOURCE_REVISION
ARG HOST_IO_REVISION
LABEL org.opencontainers.image.source="https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps" \
      org.opencontainers.image.revision="$SOURCE_REVISION" \
      org.opencontainers.image.title="Biome ToF coordinator" \
      org.biomerte.host-io.revision="$HOST_IO_REVISION"
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
WORKDIR /app
COPY deploy/tof/requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir --only-binary=:all: -r /tmp/requirements.txt \
    && rm /tmp/requirements.txt \
    && mkdir -p /app/biomerte \
    && touch /app/biomerte/__init__.py
# The reviewed core checkout is a named BuildKit context, not a forked client.
COPY --from=host_io biomerte/host_io_client.py /app/biomerte/host_io_client.py
COPY firmware/esp32c3-tof-anchor/tools/*.py /app/
HEALTHCHECK --interval=10s --timeout=3s --start-period=10s --retries=3 \
    CMD ["python", "-c", "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8771/healthz',timeout=2).read()"]
ENTRYPOINT ["python", "/app/tof_coordinator.py"]
# No active owner starts before the ToF state/checkpoint/fence qualification.
CMD ["--prepare-only", "--camera", "http://registered-camera", "--camera-id", "REGISTERED_FRONT_CAMERA_REFERENCE", "--state", "/state/tof", "--vision-state", "/state/vision", "--host-io-config", "/config/host-io.json", "--health-port", "8771", "--health-listen", "0.0.0.0"]
