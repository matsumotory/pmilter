# Pmilter: Programmable Mail Filter Server [![Build Status](https://travis-ci.org/matsumotory/pmilter.svg?branch=master)](https://travis-ci.org/matsumotory/pmilter)

Pmilter is a simple and programmable mail filter server software. You can control smtp server like postfix or sendmail via some mruby scripts. Pmilter is one-binary. So you can deploy and setup environment very easily. Enjoy!!

## milter install and run

- install

```
make mruby
make
```

- run

```
make run
```

or

```
./pmilter -c pmilter.conf
```

### build dependency (for example on ubuntu 16.04)

- build essential
- automake
- m4
- autoconf
- libtool
- cmake
- pkg-config
- libcunit1-dev
- ragel
- ruby (for mruby)
- bison (for mruby)

##### install example for ubuntu

```
sudo apt-get -y install build-essential rake bison git gperf automake m4 \
        autoconf libtool cmake pkg-config libcunit1-dev ragel
```

##### install example for CentOS7

```
yum install -y ruby gcc cc-c++ make cmake autoconf automake libtool bison
rpm -ivh ftp://ftp.pbone.net/mirror/ftp.sourceforge.net/pub/sourceforge/k/ke/kenzy/special/C7/x86_64/ragel-6.8-3.el7.centos.x86_64.rpm
```

### run dependency

very simple! :0

```
$ ldd pmilter
        linux-vdso.so.1 =>  (0x00007ffc475ed000)
        libpthread.so.0 => /lib/x86_64-linux-gnu/libpthread.so.0 (0x00007f5f81b95000)
        libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f5f8188c000)
        libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f5f814c2000)
        /lib64/ld-linux-x86-64.so.2 (0x000055ef94336000)
```

### test dependency

- ruby-milter-server
- ruby-milter-client

Thanks to [milter-manager](http://milter-manager.sourceforge.net/reference/ja/install-to.html)!!!

- install example for ubuntu

```
sudo apt-get -y install software-properties-common
sudo add-apt-repository -y ppa:milter-manager/ppa
sudo apt-get update
sudo apt-get -y install ruby-milter-server ruby-milter-client
```

### milter test after `make run`

```
make test
```

## pmilter.conf and mruby handlers

### pmilter.conf using toml

```toml
[server]
  # hoge.sock or ipaddree:port
  listen = "/var/spool/postfix/pmilter/pmilter.sock"
  timeout = 7210
  log_level = "notice"
  mruby_handler = true
  listen_backlog = 128
  debug = 0

[handler]

  [handler.config]
    # postconfig handler
    mruby_postconfig_handler = "handler/postconfig.rb"

    # master exit config handler
    mruby_master_exit_handler = "handler/master_exit.rb"

  [handler.session]
    # connection info filter handler
    mruby_connect_handler = "handler/connect.rb"
    
    # SMTP HELO command filter handler
    mruby_helo_handler = "handler/helo.rb"
    
    # envelope sender filter handler
    mruby_envfrom_handler = "handler/mail_from.rb"
    
    # envelope recipient filter handler
    mruby_envrcpt_handler = "handler/rcpt_to.rb"
    
    ## header filter handler
    mruby_header_handler = "handler/header.rb"
    
    # end of header handler
    #mruby_eoh_handler = "/path/to/handler.rb"
    
    # body block filter handler
    mruby_body_handler = "handler/body.rb"
    
    # end of message handler
    mruby_eom_handler = "handler/eom.rb"
    
    # message aborted handler
    #mruby_abort_handler = "/path/to/handler.rb"
    
    # connection cleanup handler
    #mruby_close_handler = "/path/to/handler.rb"
    
    # unknown SMTP commands handler
    #mruby_unknown_handler = "/path/to/handler.rb"
    
    ## DATA command handler
    #mruby_data_handler = "/path/to/handler.rb"
```

### mruby handler examples

- `handler/connect.rb`

```ruby
puts "hello pmilter handler called from #{Pmilter.name}"
puts "client ipaddr #{Pmilter::Session.new.client_ipaddr}"
puts "client hostname #{Pmilter::Session.new.client_hostname}"
puts "client daemon #{Pmilter::Session.new.client_daemon}"
puts "handler phase name: #{Pmilter::Session.new.handler_phase_name}"
```

- `handler/helo.rb`

```ruby
puts "helo hostname: #{Pmilter::Session.new.helo_hostname}"
puts "tls client issuer: #{Pmilter::Session.new.cert_issuer}"
puts "tls client subject: #{Pmilter::Session.new.cert_subject}"
puts "tls session key size: #{Pmilter::Session.new.cipher_bits}"
puts "tls encrypt method: #{Pmilter::Session.new.cipher}"
puts "tls version: #{Pmilter::Session.new.tls_version}"
```

- `handler/mail_from.rb`

```ruby
puts "env from from args: #{Pmilter::Session.new.envelope_from}"
puts "env from from symval: #{Pmilter::Session.new.mail_addr}"
puts "SASL login name: #{Pmilter::Session.new.auth_authen}"
puts "SASL login sender: #{Pmilter::Session.new.auth_author}"
puts "SASL login type: #{Pmilter::Session.new.auth_type}"

if Pmilter::Session.new.envelope_from == "<spam-from@example.com>"
  Pmilter.status = Pmilter::SMFIS_REJECT
end
```

- `handler/rcpt_to.rb`

```ruby
puts "env to from arg: #{Pmilter::Session.new.envelope_to}"
puts "env to from symval: #{Pmilter::Session.new.rcpt_addr}"
```

- `handler/eom.rb`

```ruby
puts "myhostname: #{Pmilter::Session.new.myhostname}"
puts "message_id: #{Pmilter::Session.new.message_id}"
puts "reveive_time: #{Time.at Pmilter::Session.new.receive_time}"
puts "add_header(X-Pmilter:True): #{Pmilter::Session::Headers.new['X-Pmilter'] = 'Enable'}"
```

- `handler/header.rb`

```ruby
puts "header: #{Pmilter::Session::Headers.new.header}"
```

- `handler/body.rb`

```ruby
puts "body chunk; #{Pmilter::Session.new.body_chunk}"

# Skip over rest of same callbacks
# only once call body handler when return Pmilter::SMFIS_SKIP
Pmilter.status = Pmilter::SMFIS_SKIP
```

### pmilter example handler run

- `make test` after `make run`

```
ubuntu@ubuntu-xenial:~/pmilter$ make run
./pmilter -c pmilter.conf
[Wed, 02 Nov 2016 12:02:11 GMT][notice]: pmilter starting
hello pmilter handler called from pmilter
client ipaddr 192.168.123.123
client hostname mx.example.net
client daemon milter-test-server
handler phase name: mruby_connect_handler
helo hostname: delian
tls client issuer: cert_issuer
tls client subject: cert_subject
tls session key size: 0
tls encrypt method: 0
tls version: 0
env from from args: <from@example.com>
env from from symval: mail_addr
SASL login name:
SASL login sender:
SASL login type:
env to from arg: <to@example.com>
env to from symval: <to@example.com>
header: {"From"=>"<from@example.com>"}
header: {"To"=>"<to@example.com>"}
header: {"Subject"=>"Hello"}
body chunk; Hello world!!
myhostname: mail.example.com
message_id: message-id
reveive_time: Wed Nov 02 21:02:15 2016
add_header(X-Pmilter:True): Enable
```

## MTA like postfix configuration example

- postfix main.cf

```
# postfix chroot on /var/spool/postfix
# create pmilter.socket as /var/spool/postfix/pmilter/pmilter.sock
smtpd_milters = unix:/pmilter/pmilter.sock
```

## Benchmarks

- don't use pmilter

```
ubuntu@ubuntu-xenial:~$ postal -t 1 -r 10000 -m 1 -M 1 127.0.0.1 mail.list
time,messages,data(K),errors,connections,SSL connections
17:39,83,119,0,84,0
17:40,1245,1774,0,1245,0
17:41,1288,1833,0,1288,0
17:42,1298,1847,0,1298,0
```

- use pmilter and callback mruby handler 10 times

```
ubuntu@ubuntu-xenial:~$ postal -t 1 -r 10000 -m 1 -M 1 127.0.0.1 mail.list
time,messages,data(K),errors,connections,SSL connections
17:45,687,979,0,688,0
17:46,1030,1467,0,1030,0
17:47,1027,1462,0,1027,0
17:48,1042,1483,0,1042,0
```

# License
under the MIT License: see also LICENSE file

* http://www.opensource.org/licenses/mit-license.php

