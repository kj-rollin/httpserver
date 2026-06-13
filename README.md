# HTTP Server in C++

A multi-threaded HTTP/1.1 server built from scratch in C++ using raw POSIX sockets — no frameworks, no external HTTP libraries. Built as a learning project to understand how web servers, authentication, and REST APIs work under the hood.

## Features

- **HTTP/1.1** request parsing (GET, POST, PUT, DELETE)
- **Multi-threaded** — one thread per connection
- **SQLite** database with prepared statements (no SQL injection)
- **Authentication** — password hashing (SHA256 + salt), session tokens with expiry
- **CSRF protection** — per-session tokens validated on state-changing requests
- **Role-based access control** — admin vs applicant roles
- **File uploads** — multipart/form-data parsing, unique filenames, DB-linked records
- **Video/audio streaming** — HTTP range requests for seekable playback
- **Response caching** — thread-safe (shared_mutex), invalidated on file change
- **Gzip compression** — automatic for responses over 1KB
- **REST API** — JSON endpoints alongside HTML pages
- **Graceful shutdown** — SIGINT/SIGTERM, waits for active requests
- **Request logging** — timestamp, IP, method, path, status

## Architecture

main.cpp       — entry point, routing setup, signal handling
router.h       — route dispatch, middleware (require_auth)
handlers.h     — request handlers (login, upload, API endpoints)
database.h     — SQLite wrapper, prepared statements
session.h      — session + CSRF token management
cache.h        — thread-safe file cache with gzip
crypto.h       — password hashing, token generation
multipart.h    — multipart/form-data parser
json.h         — minimal JSON encode/escape helpers
utils.h        — HTTP parsing helpers (path, method, cookies, etc.)
www/           — static files (HTML/CSS/JS)

## Build & Run

Requires: `clang++`, `sqlite3`, `openssl`, `zlib`

```bash
clang++ -std=c++17 main.cpp -lsqlite3 -lssl -lcrypto -lz -o mainserver
./mainserver

Or use the dev tool (auto-rebuild on file changes):
Bash

python manage.py
dev> watch

Server runs on http://localhost:8080

Database Setup
bash
sqlite3 database.db
CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL,
    password TEXT NOT NULL,
    role TEXT DEFAULT 'applicant'
);

CREATE TABLE job_applications (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL,
    filename TEXT NOT NULL,
    filepath TEXT NOT NULL,
    upload_time TEXT NOT NULL,
    status TEXT DEFAULT 'pending'
);

API Endpoints
Method
Path
Auth
Description
POST
/register
-
Create account
POST
/api/login
-
Login, returns session cookie
GET
/api/dashboard
session
Current user info + CSRF token
GET
/api/applications
session
List own uploaded applications
POST
/upload
session
Upload file (CSRF required)
DELETE
/api/applications/:id
session
Delete own application
PUT
/api/applications/:id
admin
Update application status
GET
/health
-
Health check
What I Learned
This project was built incrementally while learning C++ — starting from memory management (smart pointers, RAII) through to systems programming (sockets, threading) and full-stack concepts (sessions, CSRF, REST APIs). Each feature was added one at a time with debugging done live on an Android device via Termux.
License
MIT
