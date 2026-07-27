# CrazyPod mini-app development key

`development_ed25519.pem` is an intentionally public development signing key.
It makes the two demo packages reproducible and lets the firmware exercise the
complete signature-verification path.

It is not a production trust root. Anyone who has this repository can sign a
package accepted by a development build. A release build must embed a different
public key and keep its corresponding private key outside the repository.
