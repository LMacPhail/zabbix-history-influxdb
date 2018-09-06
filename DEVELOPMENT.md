# Local Development

You will need Zabbix sources for compiling the module. For running the module you have two options:
- compile whole Zabbix source code and run from there (follow https://www.zabbix.com/developers)
- install Zabbix from distributed packages and just put compiled module and module's config in the modules path, then restart Zabbix as usual

We are using the second option. Our local development environment is:
- VirtualBox with Ubuntu 18.04
  - added Zabbix repositories
  - installed postgresql, zabbix-server and zabbix-agent
- Independent InfluxDB instance


# Installation

Create a VM and install your Zabbix server with PostgreSQL from packages by following these instructions:

https://www.zabbix.com/documentation/3.4/manual/installation/install_from_packages/debian_ubuntu
https://www.zabbix.com/documentation/3.4/manual/installation/install#installing_frontend

> Hint: either disable SNMP monitoring or install snmp-mibs-downloader from multiverse repo to get rid of long warning in the log file after each zabbix server start.

## Get the source code

Download the zabbix source code from https://www.zabbix.com/download_sources and place it in the home dir (or any other workdir you like).

Run git clone of this repository inside of the zabbix sources under `src/modules`

```
$ cd zabbix-<version>/src/modules/
$ git clone ...
```

For development purposes create config override file under module's dir `zabbix-history-influxdb/dist/history_influxdb_local.conf` next to the regular config. This local config is where you may set any config values and avoid repository changes (it is in `.gitignore`).

Use option `ForceModuleDebugLogging=1` in the local config, this will enforce debug level logs of the module to show in the log file regardless actual log level setting.

Also put all necessities for your InfluxDB connection in the local config.

We create symlink `/usr/lib/zabbix/modules` pointing to module's `dist/`.

```
# sudo ln -s ~/zabbix-<version>/src/modules/zabbix-history-influxdb/dist /usr/lib/zabbix/modules
```

## Similarly to regular installation

Now edit the main Zabbix server configuration file, usually in `/etc/zabbix/zabbix_server.conf` and change modules section near the end to point on your module:

```
LoadModulePath=/usr/lib/zabbix/modules
...
LoadModule=history_influxdb.so
```

Restart Zabbix server daemon, usually:

```
# systemctl restart zabbix-server
```

Open Zabbix log, usually `/var/log/zabbix_server.log`. You now should see all the debug messages from the module, including potential errors.


# Compiling

You will need the packages

```
# apt install gcc libcurl4-openssl-dev libpcre3-dev libevent-dev
```

Build the module by running `make` from the module's directory, output is produced in the `dist/` subdir.

```
$ cd ~/zabbix-<version>/src/modules/zabbix-history-influxdb
$ make
```

Don't forget to restart Zabbix server daemon after each build.
