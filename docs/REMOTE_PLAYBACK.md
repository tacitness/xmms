# Remote playback

`xmms-remote` mirrors the local XMMS session to an XMMS instance on a
remote desktop. It uses SSH's Unix-domain socket forwarding to reach the remote
XMMS control socket. The control protocol is not opened on a TCP port.

## Requirements

- SSH key access to the remote account, with `ssh` and `scp` available locally.
- XMMS installed on the remote host when launching a new player.
- The remote account must own the target X desktop and its audio session.
- The local and remote XMMS builds must use compatible control protocols.

## Mirror playback

```sh
xmms-remote --host jmh --display :0
```

This attaches to remote XMMS session 0, or starts XMMS on display `:0` if the
session is absent. It follows local session 0 until interrupted. The full
playlist and its current selection are synchronized. Only the selected local
file is copied with `scp` to a private directory under `/tmp` on the remote host;
the remote player reads the copy. URI tracks are passed through for the remote
player to open itself. Track changes, play/pause/stop state, and playback
position are mirrored. Position is corrected when drift exceeds 2.5 seconds.

Open **Remote Playback Control** from XMMS's General menu, or run
`xmms-remote-control` to open the button panel. Choose **Yoda is master**
or **JMH is master** to switch which player controls the other; choose **Stop
remote playback and client** to stop JMH playback and synchronization while
leaving both players open.
The panel expects SSH aliases `jmh` and `yoda` to work in both directions.

To launch a user-local build that is not on the remote `PATH`, pass its binary
path explicitly, for example `--remote-xmms /home/joel/.local/opt/xmms-ng/bin/xmms`.
Automatic launch currently targets session 0; `--session N` can attach to an
already running nonzero session.

Use `--once` to mirror the current state and exit. Use `--attach` to refuse to
launch a player when the requested remote session is absent. The display and
session can be selected explicitly:

```sh
xmms-remote --host jmh --display :0 --session 0 --source-session 0 --once
```

If the local player is stopped but has a selected track, add `--start-current`
to start that selection remotely while leaving the local player stopped. In
continuous mode the helper then follows later local playback and track changes.

## Control a remote session directly

Pass XMMS command-line options after `--`; the local XMMS binary sends them
through the same private SSH tunnel:

```sh
xmms-remote --host jmh --attach -- --pause
xmms-remote --host jmh --attach -- --play
```

To use the skin GUI, launch or attach to the remote session and interact with
its window on the selected display. The remote process uses the remote user's
Xauthority, D-Bus session bus, and PulseAudio-compatible socket under
`/run/user/<uid>`.

The synchronizer copies only the currently selected local file. It does not
copy a whole playlist or delete older copies from the remote temporary
directory. Streams and other URI sources are passed through and therefore need
to be reachable and supported by the remote player's input plugins.
