iOS Push Email for Dovecot
==========================

What is this?
-------------

This project, together with the [dovecot-xaps-daemon](https://github.com/st3fan/dovecot-xaps-daemon) project, will enable push email for iOS devices that talk to your Dovecot 2.0.x IMAP server. This is specially useful for people who are migrating away from running email services on OS X Server and want to keep the Push Email ability.

> Please note that it is not possible to use this project without legally owning a copy of OS X Server. You can purchase OS X Server on the [Mac App Store](https://itunes.apple.com/ca/app/os-x-server/id714547929?mt=12) or download it for free if you are a registered Mac or iOS developer.

High Level Overview
-------------------

There are two parts to enabling iOS Push Email. You will need both parts for this to work.

First you need to install the Dovecot plugins from this project. The Dovecot plugins add support for the `XAPPLEPUSHSERVICE` IMAP extension that will let iOS devices register themselves to receive native push notifications for new email arrival.

(Apple did not document this feature, but it did publish the source code for all their Dovecot patches on the [Apple Open Source project site](http://www.opensource.apple.com/source/dovecot/dovecot-293/), which include this feature. So although I was not able to follow a specification, I was able to read their open source project and do a clean implementation with all original code.)

Second, you need to install a daemon process, from the [dovecot-xaps-plugin](https://github.com/st3fan/dovecot-xaps-daemon) project, that will be responsible for receiving new email notifications from the Dovecot Local Delivery Agent or from the Dovecot LMTP server and transforming those into native Apple Push Notifications.

Installation
============

Prerequisites
-------------

You are going to need the following things to get this going:

* Some patience and willingness to experiment - Although I run this project in production, it is still a very early version and it may contain bugs.
* Because you will need a certificate to talk to the Apple Push Notifications Service, you can only run this software if you are migrating away from an existing OS X Server setup where you had Push Email enabled. How to export the certificate is described in the [dovecot-xaps-daemon project](https://github.com/st3fan/dovecot-xaps-daemon).
* This software has only been tested on Ubuntu 12.04.5 with Dovecot 2.0.19. So ideally you have a mail server with the same specifications, or something very similar.

> Note that you need to have an existing Dovecot setup working. Either with local system users or with virtual users. Also note that you need to be using the Dovecot Local Delivery Agent or the Dovecot LMTP server for this to work. The [Dovecot LDA](http://wiki2.dovecot.org/LDA) and the [LMTP server](http://wiki2.dovecot.org/LMTP) are described in detail on the Dovecot Wiki

Installing the Dovecot plugins
------------------------------

First install the following Ubuntu 12.04.5 packages, or equivalent for your operating system. This list is longer than it should be because there is not yet a binary distribution for this project.

```
sudo apt-get build-dep dovecot-core
sudo apt-get install git dovecot-dev
```

Clone this project:

```
git clone https://github.com/st3fan/dovecot-xaps-plugin.git
cd dovecot-xaps-plugin
```

Compile and install the plugins. Note that the installation destination in the `Makefile` is hardcoded for Ubuntu, it expects the Dovecot modules to live at `/usr/lib/dovecot/modules/`. You can either modify the `Makefile` or copy the modules to the right place manually.

```
make
sudo make install
```

Install the configuration file. Also specific for Ubuntu, may be different for your operating system.

```
sudo cp xaps.conf /etc/dovecot/conf.d/95-xaps.conf
```

In the configuration file, change the `xaps_socket` option to point to the same location as you specified on the `xapsd` daemon arguments.

Restart Dovecot:

```
sudo service dovecot restart
```

Debugging
---------

Put a tail on `/var/log/mail.log` and keep an eye on the output of the `xapsd` daemon. (See instructions in that project). If you see any errors or core dumps, please [file a bug](https://github.com/st3fan/dovecot-xaps-plugin/issues/new).

