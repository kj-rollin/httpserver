# High-Performance C++ HTTP/1.1 Server

A robust, multi-threaded HTTP/1.1 server built from the ground up using **raw POSIX sockets** and C++17. This project implements core web server functionalities—including request routing, session management, security protections, and database integration—without the use of high-level web frameworks or external HTTP libraries.

---

## 🚀 Key Features

### 1. Custom HTTP/1.1 Networking Stack
Built on top of raw TCP sockets (`AF_INET`, `SOCK_STREAM`), the server handles the full lifecycle of an HTTP request:
- **Request Parsing**: Manual extraction of HTTP methods (GET, POST, PUT, DELETE), paths, query strings, and headers.
- **Protocol Compliance**: Support for `Content-Length` framing and `Connection: close` semantics.
- **Multipart Parsing**: A custom parser for `multipart/form-data`, enabling high-performance file uploads.

### 2. Multi-Threaded Architecture
The server utilizes a **custom Thread Pool** implementation to handle concurrent connections efficiently:
- **Worker Pattern**: A fixed-size pool of worker threads waits on a task queue, minimizing the overhead of thread creation.
- **Graceful Shutdown**: Sophisticated signal handling (`SIGINT`/`SIGTERM`) ensures that the server waits for active requests to complete before exiting.
- **Thread-Safe Shared State**: Uses `std::shared_mutex` and `std::mutex` for thread-safe access to the file cache, session store, and database.

### 3. Security & Authentication
Security is integrated into the core architecture rather than added as an afterthought:
- **Session Management**: Secure, token-based session tracking with automatic 30-minute expiry.
- **CSRF Protection**: Per-session CSRF tokens are validated for all state-changing operations (POST/PUT/DELETE).
- **Password Security**: Implements **SHA-256 hashing with unique per-user salts** to protect against rainbow table attacks.
- **Role-Based Access Control (RBAC)**: Middleware-style wrappers (`require_auth`) protect sensitive routes based on user authentication and roles.

### 4. Performance Optimizations
- **LRU File Cache**: A thread-safe cache stores frequently accessed static files in memory to reduce disk I/O.
- **Gzip Compression**: Automatic Gzip compression for all text-based responses exceeding 1KB, significantly reducing bandwidth usage.
- **Rate Limiting**: An IP-based rate limiter prevents Denial-of-Service (DoS) attacks by capping requests per time window.

### 5. Data & Storage
- **SQLite Integration**: Persistent storage using SQLite with **prepared statements** to prevent SQL injection vulnerabilities.
- **File Management**: Secure file upload system that generates unique filenames to prevent collisions and overwrites.

---

## 🛠 Architecture Overview

| Component | Responsibility |
| :--- | :--- |
| **`main.cpp`** | Entry point, socket initialization, and main accept loop. |
| **`router.h`** | High-level route dispatching and middleware management. |
| **`handlers.h`** | Business logic for API endpoints and page rendering. |
| **`database.h`** | Thread-safe SQLite wrapper and data persistence layer. |
| **`session.h`** | Management of user sessions and CSRF tokens. |
| **`cache.h`** | In-memory file cache with Gzip integration. |
| **`crypto.h`** | Cryptographic primitives for hashing and token generation. |
| **`utils.h`** | Low-level HTTP parsing and socket utility functions. |

---

## 📦 Build & Installation

### Prerequisites
- **Compiler**: Clang++ or G++ (C++17 support required)
- **Libraries**: `libsqlite3`, `libssl`, `libcrypto`, `zlib`

### Build Instructions
```bash
# Clone the repository
git clone https://github.com/kj-rollin/httpserver
cd httpserver


#Database Setup

bash
`sqlite3 database.db`

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

## Compile and run the server

```bash
clang++ -std=c++17 main.cpp -lsqlite3 -lssl -lcrypto -lz -o mainserver
./mainserver

Or use the dev tool (auto-rebuild on file changes):
Bash

python manage.py
dev> watch

Server runs on http://localhost:8080

```

---

## 📡 API Endpoints

| Method | Endpoint | Access | Description |
| :--- | :--- | :--- | :--- |
| `POST` | `/register` | Public | User registration |
| `POST` | `/api/login` | Public | Login & Session initialization |
| `GET` | `/api/dashboard` | User | User profile & CSRF token retrieval |
| `POST` | `/upload` | User | Multipart file upload |
| `DELETE` | `/api/applications/:id` | User | Delete personal application |
| `PUT` | `/api/applications/:id` | Admin | Update application status |

---

## 📜 License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing
This project was built incrementally while learning C++ — starting from memory management (smart pointers, RAII) through to systems programming (sockets, threading) and full-stack concepts (sessions, CSRF, REST APIs). Each feature was added one at a time with debugging done live on
Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.
