#!/usr/bin/env python3
"""Move a Hermes kanban task to another status (direct SQLite update + audit event).

Called by KatHub backend when the user drags a card between columns:
    python kanban_move.py <task_id> <new_status>

Outputs JSON: {"ok": true, "id": ..., "from": ..., "to": ...} or {"ok": false, "error": ...}
"""
import json
import os
import sqlite3
import sys
import time

VALID_STATUSES = {"triage", "todo", "scheduled", "ready", "running", "blocked", "review", "done", "archived"}


def db_path():
    home = os.environ.get("LOCALAPPDATA") or os.path.expanduser("~/AppData/Local")
    return os.path.join(home, "hermes", "kanban", "boards", "kathub", "kanban.db")


def main():
    if len(sys.argv) < 3:
        print(json.dumps({"ok": False, "error": "usage: kanban_move.py <task_id> <new_status>"}))
        return 1
    task_id, new_status = sys.argv[1], sys.argv[2]

    if new_status not in VALID_STATUSES:
        print(json.dumps({"ok": False, "error": "invalid status: " + new_status}))
        return 1

    db = db_path()
    if not os.path.isfile(db):
        print(json.dumps({"ok": False, "error": "kanban.db not found: " + db}))
        return 1

    conn = sqlite3.connect(db)
    try:
        row = conn.execute("SELECT id, status FROM tasks WHERE id = ?", (task_id,)).fetchone()
        if not row:
            print(json.dumps({"ok": False, "error": "task not found: " + task_id}))
            return 1
        old_status = row[1]
        now = int(time.time())

        # Derive timestamps so the board stays consistent
        started_at = None
        completed_at = None
        if new_status == "running":
            started_at = now
        elif new_status == "done":
            completed_at = now

        conn.execute(
            "UPDATE tasks SET status = ?, started_at = COALESCE(?, started_at), completed_at = ? WHERE id = ?",
            (new_status, started_at, completed_at, task_id),
        )
        conn.execute(
            "INSERT INTO task_events (task_id, kind, payload, created_at) VALUES (?, 'status_change', ?, ?)",
            (task_id, json.dumps({"from": old_status, "to": new_status, "source": "kathub-ui"}), now),
        )
        conn.commit()
        print(json.dumps({"ok": True, "id": task_id, "from": old_status, "to": new_status}))
        return 0
    except Exception as e:  # noqa: BLE001
        conn.rollback()
        print(json.dumps({"ok": False, "error": str(e)}))
        return 1
    finally:
        conn.close()


if __name__ == "__main__":
    sys.exit(main())
