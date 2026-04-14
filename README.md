# 🔐 OS-Jackfruit: Multi-Container Runtime

## 📌 Overview

This project implements a **lightweight container runtime in C** that demonstrates core OS concepts like **process isolation, namespace management, and kernel interaction**.

It focuses on essential primitives such as **Linux namespaces, chroot-based filesystem isolation, and a supervisor daemon**, without the complexity of full container platforms like Docker.

---

## 🚀 Features

* Namespace-based process isolation  
* Filesystem isolation using `chroot()`  
* Supervisor daemon for container lifecycle management  
* Real-time logging (stdout + file)  
* Kernel module for memory monitoring  
* Background and interactive container execution  
* Multi-container support  

---

## 🏗️ Architecture

Containers are created and managed through a user-space runtime interacting with a kernel module.


Client (CLI)
↓
Supervisor (engine)
↓
Container Process (namespaces + chroot)
↓
Log Manager (pipe + fork)
↓
Kernel Module (monitor.ko)
↓
/dev/container_monitor


---

## 📁 Project Structure


boilerplate/
├── engine.c
├── monitor.c
├── monitor_ioctl.h
├── Makefile
├── cpu_hog.c
├── memory_hog.c
├── io_pulse.c
└── environment-check.sh

rootfs-alpha/
rootfs-beta/
README.md


---

## 🛠️ Setup & Installation

### Requirements

* Ubuntu 22.04 / 24.04  
* build-essential  
* linux-headers  
* Secure Boot OFF  
* WSL not supported  

---

### Installation

```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r)
cd boilerplate
chmod +x environment-check.sh
sudo ./environment-check.sh
Root Filesystem Setup
mkdir rootfs-base

wget https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz

tar -xzf alpine-minirootfs-3.20.3-x86_64.tar.gz -C rootfs-base

cp -a ./rootfs-base ./rootfs-alpha
cp -a ./rootfs-base ./rootfs-beta
Build
cd boilerplate
make

▶️ Usage
Run Container (Interactive)
sudo ./engine run alpha ../rootfs-alpha
Start Container (Background)
sudo ./engine start alpha ../rootfs-alpha
List Containers
./engine ps
Stop Container
sudo ./engine stop alpha
View Logs
cat ../rootfs-alpha/logs/alpha.log

🧠 Kernel Module
Load / Unload
sudo insmod monitor.ko
sudo rmmod monitor
Check Device
ls /dev/container_monitor
View Kernel Logs
dmesg | tail -20

🧪 Workloads
cpu_hog → CPU intensive
memory_hog → memory stress
io_pulse → I/O workload
./cpu_hog
./memory_hog
./io_pulse

💡 Summary

This project demonstrates how containers work internally using:

Linux namespaces
Process isolation
Kernel-user communication
Basic runtime design
