# hostd

VERSION: alpha.

This is a strawman version.  Just enough to compile.

This is for the reciprecal part of vim-cmd.  It will act as a contact point which instantiates vmlib.

ALL OF THIS IS EXPERIMENTAL

## INSTALLATION

```bash
cd /opt
git clone https://github.com/tlh45342/hostd.git
cd mtools
make ; make install
```

## SERVICE.D

Please note the following systemctl commands for controlling the service after using "make insall:
which will install the appropriate hostd.service file.

```bash
sudo systemctl enable hostd 
sudo systemctl start hostd
```

The following may be used to check on the "hostd" service status

```bash
sudo systemctl status hostd
```

journalctl -u hostd -f

sudo systemctl daemon-reload
