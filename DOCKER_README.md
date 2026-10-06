# Containers

The container definitions in [`docker/`](docker/) build this fork from the current checkout. They are intended for development and self-hosted deployments; this repository does not direct users to images published by another Sunshine project.

Available definitions cover Debian Bookworm, Fedora 39 and 40, and Ubuntu 22.04 and 24.04. Each definition builds Sunshine, runs its native test binary, produces a distribution package, and installs that package into the final image.

## Runtime requirements

- Expose TCP ports `47984-47990` and `48010`.
- Expose UDP ports `47998-48000`.
- Persist `/config`.
- Pass the relevant `/dev/dri` device on Linux when hardware capture or encoding requires it.
- Set `PUID`, `PGID`, and `TZ` for the deployment environment.

Example runtime shape:

```bash
docker run -d \
  --device /dev/dri/ \
  --name sunshine \
  --restart unless-stopped \
  --ipc host \
  -e PUID=1001 \
  -e PGID=1001 \
  -e TZ=America/New_York \
  -v /path/to/sunshine-data:/config \
  -p 47984-47990:47984-47990/tcp \
  -p 48010:48010 \
  -p 47998-48000:47998-48000/udp \
  foundation-sunshine:local
```

The final image name is deliberately a placeholder. Build and tag one of this repository's Dockerfiles for the target distribution before running it. Containerized capture, device access, and desktop-session integration remain host-specific, so verify those capabilities on the deployment host.
