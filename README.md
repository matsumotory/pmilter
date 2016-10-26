# [PoC] pmilter handling by mruby

## dependency (on ubuntu 16.04)

- build essential
- automake
- m4
- autoreconf
- autoconf
- libtool
- automake
- cmake
- pkg-config
- libcunit1-dev
- libicu-dev


## milter install and run

```
make
make run
```

## milter test after `make run`

```
make test
```

### pmilter output debug meesages

```
ubuntu@ubuntu-xenial:~/pmilter$ make run # make test by other terminal
------------
mrb_xxfi_negotiate
mrb_xxfi_connect
    hostname: mx.example.net
    info->ipaddr: 192.168.123.123
    info->connect_daemon: milter-test-server
    if_name: localhost
    if_addr: 127.0.0.1
    j: mail.example.com
    _: (null)
mrb_xxfi_helo
    helohost: delian
    tls_version: 0
    cipher: 0
    cipher_bits: 0
    cert_subject: cert_subject
    cert_issuer: cert_issuer
mrb_xxfi_envfrom
    info->envelope_from: <from@example.com>
    i: i
    auth_type: (null)
    auth_authen: (null)
    auth_ssf: (null)
    auth_author: (null)
    mail_mailer: mail_mailer
    mail_host: mail_host
    mail_addr: mail_addr
mrb_xxfi_envrcpt
    info->envelope_to: <to@example.com>
    argv[0]: <to@example.com>
    rcpt_mailer: rcpt_mailer
    rcpt_host: rcpt_host
    rcpt_addr: <to@example.com>
mrb_xxfi_data
mrb_xxfi_header
    headerf: From
    headerv: <from@example.com>
mrb_xxfi_header
    headerf: To
    headerv: <to@example.com>
mrb_xxfi_header
    headerf: Subject
    headerv: Hello
mrb_xxfi_eoh
mrb_xxfi_body
    body: Hello world!!

mrb_xxfi_eom
    info->receive_time: 1477305542
    msg_id: (null)
mrb_xxfi_close
```
