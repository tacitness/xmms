# XMMS Package Repositories

Signed apt / dnf / apk repositories published to the TacitSoft S3 + CloudFront
distribution by [`release-publish.yml`](../.github/workflows/release-publish.yml)
via [`scripts/publish-repo.sh`](../scripts/publish-repo.sh).

## End-user installation

> Replace `packages.tacitsoft.dev` with the configured `PACKAGE_REPO_BASE_URL`.

### Debian / Ubuntu (apt)

```bash
curl -fsSL https://packages.tacitsoft.dev/xmms-archive-keyring.asc \
  | sudo gpg --dearmor -o /usr/share/keyrings/xmms-archive-keyring.gpg
echo "deb [signed-by=/usr/share/keyrings/xmms-archive-keyring.gpg] https://packages.tacitsoft.dev/deb stable main" \
  | sudo tee /etc/apt/sources.list.d/xmms.list
sudo apt-get update && sudo apt-get install xmms
```

### Fedora / RHEL (dnf)

```bash
sudo curl -fsSL https://packages.tacitsoft.dev/rpm/xmms.repo -o /etc/yum.repos.d/xmms.repo
sudo dnf install xmms
```

### Alpine (apk)

```bash
curl -fsSL https://packages.tacitsoft.dev/xmms-archive-keyring.rsa.pub \
  | sudo tee /etc/apk/keys/xmms-archive-keyring.rsa.pub
echo "https://packages.tacitsoft.dev/apk" | sudo tee -a /etc/apk/repositories
sudo apk update && sudo apk add xmms
```

## Operator setup (one-time)

1. **Archive signing key** — generate an offline GPG key for the repository:
   ```bash
   gpg --quick-generate-key "TacitSoft XMMS Archive <packages@tacitsoft.dev>" rsa4096 sign never
   gpg --armor --export-secret-keys <key-id>   # → store as the PACKAGE_SIGNING_KEY secret
   ```
   For Alpine, also generate an `abuild` key (`abuild-keygen -a`) and store the
   private key as the `ABUILD_PRIVKEY` secret; publish the `.rsa.pub`.

2. **AWS** — create the S3 bucket + CloudFront distribution (origin = bucket),
   and an IAM role trusted by GitHub OIDC with `s3:PutObject`/`s3:DeleteObject`
   on the bucket and `cloudfront:CreateInvalidation` on the distribution.
   (Provision via dagobah-infra Terraform alongside the other static sites.)

3. **GitHub config** — set the repo variables and secrets listed in the
   workflow header (`PACKAGE_REPO_BUCKET`, `PACKAGE_REPO_BASE_URL`,
   `PACKAGE_REPO_CF_DIST`, `AWS_REGION`, `AWS_ROLE_TO_ASSUME`,
   `PACKAGE_SIGNING_KEY`, `ABUILD_PRIVKEY`).

4. **Validate** — `workflow_dispatch` the publish with `dry_run: true` against an
   existing tag; confirm the repo tree is built before a real publish.

## Local dry run

```bash
STAGING_DIR=dist GPG_KEY_ID=<key> \
  S3_BUCKET=example REPO_BASE_URL=https://example \
  bash scripts/publish-repo.sh        # PUBLISH unset → dry run, no S3
```

## Security notes

- The archive private key never lives in the repo; it is injected at publish
  time from the `PACKAGE_SIGNING_KEY` secret and used only to sign metadata.
- apt verifies the `Release` signature, dnf verifies `repomd.xml.asc`
  (`repo_gpgcheck=1`), apk verifies the signed `APKINDEX`.
- AWS access is short-lived via OIDC — no long-lived keys in CI.
