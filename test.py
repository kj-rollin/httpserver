import requests
import sys

BASE = "http://localhost:8080"
PASS = "\033[92m✅ PASS\033[0m"
FAIL = "\033[91m❌ FAIL\033[0m"

passed = 0
failed = 0

def test(name, condition, detail=""):
    global passed, failed
    if condition:
        print(f"{PASS} {name}")
        passed += 1
    else:
        print(f"{FAIL} {name} {detail}")
        failed += 1

def safe_json(r):
    try:
        return r.json()
    except Exception:
        return {}

def summary():
    print(f"\n{'='*40}")
    print(f"Results: {passed} passed, {failed} failed")
    print(f"{'='*40}")
    sys.exit(0 if failed == 0 else 1)

def login(username, password):
    s = requests.Session()
    s.post(f"{BASE}/api/login",
           data=f"username={username}&password={password}",
           headers={"Content-Type": "application/x-www-form-urlencoded"})
    return s

# ─── health check ─────────────────────────
def test_health():
    print("\n[Health Check]")
    r = requests.get(f"{BASE}/health")
    test("GET /health returns 200", r.status_code == 200)
    test("GET /health returns JSON", "status" in safe_json(r))
    test("status is ok", safe_json(r).get("status") == "ok")

# ─── authentication ────────────────────────
def test_auth():
    print("\n[Authentication]")
    s = requests.Session()

    # invalid login
    r = s.post(f"{BASE}/api/login",
               data="username=nobody&password=wrong",
               headers={"Content-Type": "application/x-www-form-urlencoded"})
    test("invalid login returns 401", r.status_code == 401)
    test("invalid login returns success=false", safe_json(r).get("success") == False)

    # valid login
    r = s.post(f"{BASE}/api/login",
               data="username=admin&password=1234",
               headers={"Content-Type": "application/x-www-form-urlencoded"})
    test("valid login returns 200", r.status_code == 200)
    test("valid login returns success=true", safe_json(r).get("success") == True)
    test("valid login returns username", safe_json(r).get("username") == "admin")
    test("valid login returns csrf_token", "csrf_token" in safe_json(r))

    # dashboard with valid session
    r = s.get(f"{BASE}/api/dashboard")
    test("dashboard with session returns 200", r.status_code == 200)
    test("dashboard returns username", safe_json(r).get("username") == "admin")

    # dashboard without session
    r = requests.get(f"{BASE}/api/dashboard")
    test("dashboard without session returns 401", r.status_code == 401)
    test("dashboard without session returns error", "error" in safe_json(r))

# ─── authorization ─────────────────────────
def test_authorization():
    print("\n[Authorization]")

    admin = login("admin", "1234")

    # register testuser if not exists
    requests.post(f"{BASE}/register",
                  data="username=testuser&password=test123",
                  headers={"Content-Type": "application/x-www-form-urlencoded"})

    testuser = login("testuser", "test123")
    r = testuser.get(f"{BASE}/api/dashboard")
    testuser_logged_in = r.status_code == 200

    # admin can update application status
    r = admin.put(f"{BASE}/api/applications/1",
                  data='{"status":"reviewed"}',
                  headers={"Content-Type": "application/json"})
    j = safe_json(r)
    test("admin can PUT application status",
         r.status_code == 200 and
         (j.get("success") == True or j.get("error") == "Application not found"))

    # regular user cannot update status
    if testuser_logged_in:
        r = testuser.put(f"{BASE}/api/applications/1",
                         data='{"status":"reviewed"}',
                         headers={"Content-Type": "application/json"})
        test("non-admin cannot PUT application",
             r.status_code == 403)
    else:
        test("testuser login failed", False, "(check registration)")

# ─── API endpoints ─────────────────────────
def test_api():
    print("\n[API Endpoints]")
    s = login("admin", "1234")
    r_login = s.post(f"{BASE}/api/login", data="username=admin&password=1234", headers={"Content-Type": "application/x-www-form-urlencoded"})
    print(f"Login status: {r_login.status_code}, cookies: {dict(s.cookies)}")

    # applications list
    r = s.get(f"{BASE}/api/applications")
    test("GET /api/applications returns 200", r.status_code == 200)
    test("GET /api/applications returns list", isinstance(safe_json(r), list))

    # delete with invalid ID
    r = s.delete(f"{BASE}/api/applications/abc")
    print(f"DELETE abc status: {r.status_code}, body: {r.text[:100]}")
    test("DELETE with invalid ID returns success=false",
         safe_json(r).get("success") == False)

    # PUT with invalid ID
    r = s.put(f"{BASE}/api/applications/xyz",
              data='{"status":"reviewed"}',
              headers={"Content-Type": "application/json"})
    test("PUT with invalid ID handled safely",
         r.status_code in [200, 400, 403])

    # GET single non-existent application
    r = s.delete(f"{BASE}/api/applications/999999")
    print(f"DELETE 999999 status: {r.status_code}, body: {r.text[:100]}")
    test("DELETE non-existent returns success=false",
         safe_json(r).get("success") == False)

# ─── security ──────────────────────────────
def test_security():
    print("\n[Security]")

    # rate limiting — normal requests allowed
    responses = [requests.get(f"{BASE}/health") for _ in range(5)]
    codes = [r.status_code for r in responses]
    test("normal requests not rate limited", all(c == 200 for c in codes))

    # 404 for missing page
    r = requests.get(f"{BASE}/nonexistent_xyz.html")
    test("missing page returns 404", r.status_code == 404)

    # dashboard requires auth
    r = requests.get(f"{BASE}/dashboard.html", allow_redirects=False)
    test("dashboard.html redirects without auth", r.status_code == 302)

    # XSS payload rejected at login
    r = requests.post(f"{BASE}/api/login",
                      data="username=<script>alert('xss')</script>&password=test",
                      headers={"Content-Type": "application/x-www-form-urlencoded"})
    test("XSS payload login returns 401", r.status_code == 401)

    # SQL injection attempt
    r = requests.post(f"{BASE}/api/login",
                      data="username=admin'--&password=anything",
                      headers={"Content-Type": "application/x-www-form-urlencoded"})
    test("SQL injection attempt rejected", r.status_code == 401)

# ─── run all ───────────────────────────────
if __name__ == "__main__":
    print("🧪 HealWell HTTP Server Test Suite")
    print("="*40)
    test_health()
    test_auth()
    test_authorization()
    test_api()
    test_security()
    summary()
