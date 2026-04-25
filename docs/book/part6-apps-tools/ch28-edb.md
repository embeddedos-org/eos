# Chapter 28: eDB — Unified Multi-Model Database

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 28.1 Introduction

**eDB** is a unified multi-model database that combines SQL, Document/NoSQL,
and Key-Value storage in a single embedded engine. It includes a Python
backend (FastAPI + SQLite), a React/TypeScript frontend with SQL editor
and AI-powered query assistance, and a standalone browser version.

---

## 28.2 Three Data Models in One

| Model           | API                    | Use Case                        |
|-----------------|------------------------|---------------------------------|
| **SQL**         | `db.sql.*`             | Structured data, relationships  |
| **Document**    | `db.docs.*`            | Flexible JSON documents         |
| **Key-Value**   | `db.kv.*`              | Cache, sessions, config         |

### Usage Example

```python
from edb.core.database import Database
from edb.core.models import ColumnDefinition, ColumnType, TableSchema

db = Database("my_app.db")

# SQL
schema = TableSchema(name="users", columns=[
    ColumnDefinition(name="id", col_type=ColumnType.INTEGER, primary_key=True),
    ColumnDefinition(name="name", col_type=ColumnType.TEXT),
])
db.sql.create_table(schema)
db.sql.insert("users", {"id": 1, "name": "Alice"})

# Documents
db.docs.insert("logs", {"event": "login", "user": "Alice"})

# Key-Value
db.kv.set("session:abc", {"user_id": 1}, ttl=3600)
```

---

## 28.3 Architecture

```
eDB/
├── src/
│   ├── edb/                  # Python backend
│   │   ├── api/              # FastAPI routes and dependencies
│   │   ├── auth/             # JWT, users, RBAC
│   │   ├── ebot/             # AI/NLP query interface
│   │   ├── query/            # Query parser and planner
│   │   ├── security/         # Encryption, audit, input validation
│   │   ├── config.py         # Pydantic Settings
│   │   └── cli.py            # CLI entry point
│   ├── components/           # React UI components
│   │   ├── TopBar.tsx
│   │   ├── TableList.tsx
│   │   ├── TableView.tsx     # Data grid with inline editing
│   │   ├── QueryEditor.tsx   # SQL query editor
│   │   └── EBotSidebar.tsx   # AI assistant sidebar
│   └── hooks/                # React hooks
├── browser/
│   └── edb.html              # Standalone browser version
└── tests/
```

---

## 28.4 Quick Start

### Backend

```bash
git clone https://github.com/embeddedos-org/eDB.git
cd eDB
pip install -e ".[dev]"
edb init
edb admin create --username admin
edb serve --port 8000
# API docs at http://localhost:8000/docs
```

### Frontend

```bash
npm install
npm run dev
# React UI at http://localhost:5178
```

### Browser Standalone

Open `browser/edb.html` directly in any browser for a zero-install
experience with localStorage persistence.

### Interactive Shell

```bash
edb shell
# edb> SELECT * FROM users
# edb> .tables
# edb> .collections
```

---

## 28.5 Security Features

| Feature               | Description                              |
|-----------------------|------------------------------------------|
| **JWT Authentication**| Access and refresh tokens                |
| **RBAC**              | Admin, read_write, read_only roles       |
| **AES-256 Encryption**| Field-level encryption at rest (GCM)     |
| **Audit Logging**     | Tamper-resistant logs with hash chain    |
| **Input Sanitization**| SQL, NoSQL, and prompt injection defense |

### Required Environment Variables

| Variable              | Description                              |
|-----------------------|------------------------------------------|
| `EDB_JWT_SECRET`      | Secret key for JWT signing               |
| `EDB_ENCRYPTION_KEY`  | Key for AES-256-GCM encryption           |
| `EDB_CORS_ORIGINS`    | Comma-separated allowed origins          |

---

## 28.6 eBot AI Query Assistant

eBot translates natural language queries into SQL or NoSQL operations:

```
User: "Show me all users who signed up last month"
eBot: SELECT * FROM users WHERE created_at >= '2026-03-01'
```

Current implementation uses rule-based NLP. The roadmap includes
LLM-powered query generation via OpenAI or local models.

---

## 28.7 REST API

eDB exposes a full CRUD REST API via FastAPI:

| Endpoint                | Method | Description                |
|-------------------------|--------|----------------------------|
| `/api/tables`           | GET    | List all tables            |
| `/api/tables`           | POST   | Create a table             |
| `/api/tables/{name}`    | GET    | Get table data             |
| `/api/tables/{name}`    | DELETE | Drop a table               |
| `/api/query`            | POST   | Execute SQL query          |
| `/api/docs/{collection}`| POST   | Insert document            |
| `/api/kv/{key}`         | GET    | Get key-value pair         |
| `/api/kv/{key}`         | PUT    | Set key-value pair         |
| `/admin/audit`          | GET    | View audit logs            |

Auto-generated OpenAPI documentation is available at `/docs`.

---

## 28.8 React Frontend

The React frontend provides:

- **Table Navigator** — sidebar listing all tables and collections
- **Data Grid** — inline editing with validation
- **SQL Editor** — syntax-highlighted query editor with results
- **eBot Sidebar** — natural language query interface
- **Status Bar** — connection status, row count, query timing

---

## 28.9 Configuration

Configure via environment variables (prefix `EDB_`) or `.env` file:

```bash
EDB_DB_PATH=my_data.db
EDB_API_HOST=0.0.0.0
EDB_API_PORT=8000
EDB_JWT_SECRET=your-strong-secret-here
EDB_ENCRYPTION_KEY=your-encryption-key
EDB_CORS_ORIGINS=http://localhost:3000,https://yourdomain.com
```

---

## 28.10 Roadmap

- [x] Core multi-model engine (SQL, Document, KV)
- [x] JWT authentication and RBAC
- [x] AES-256 encryption at rest
- [x] REST API (FastAPI)
- [x] React frontend with SQL editor
- [x] Browser standalone version
- [ ] LLM-powered eBot
- [ ] Graph data model
- [ ] Multi-node clustering
- [ ] GraphQL and gRPC interfaces

---

## 28.11 Summary

eDB provides a zero-dependency embedded database with three data models,
enterprise security, and an AI-powered query assistant.

**Key takeaways:**

- Three data models (SQL, Document, KV) in one SQLite-backed engine
- JWT auth, RBAC, AES-256 encryption, tamper-resistant audit logs
- React frontend with inline editing and SQL editor
- eBot natural language query assistant
- Zero external database dependencies
- Browser standalone version for zero-install usage

---

*Next: Chapter 29 — eBrowser*
