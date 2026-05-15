package main

import (
	"fmt"
	"html/template"
	"log"
	"net/http"
	"sync"
	"time"
)

// --- In-memory store ---

var (
	mu          sync.RWMutex
	message     = "HELLO FROM FUJINET!"
	lastUpdated = time.Now()
)

// --- HTML template ---

const indexHTML = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>FujiNet Display Terminal</title>

  <!-- Google Fonts: Share Tech Mono for that terminal feel -->
  <link rel="preconnect" href="https://fonts.googleapis.com" />
  <link
    href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=VT323&display=swap"
    rel="stylesheet"
  />

  <style>
    /* ── Root / Colour tokens ─────────────────────────────────────── */
    :root {
      --green:        #39ff14;
      --green-dim:    #1a7a08;
      --green-bright: #aaff80;
      --bg:           #040a02;
      --bg-panel:     #060e04;
      --glow:         0 0 8px #39ff14, 0 0 20px #39ff1466;
      --glow-strong:  0 0 12px #39ff14, 0 0 40px #39ff1488, 0 0 80px #39ff1433;
      --font-mono:    'Share Tech Mono', monospace;
      --font-display: 'VT323', monospace;
    }

    /* ── Reset ────────────────────────────────────────────────────── */
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    /* ── Body / CRT background ────────────────────────────────────── */
    body {
      min-height: 100vh;
      background-color: var(--bg);
      color: var(--green);
      font-family: var(--font-mono);
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: flex-start;
      padding: 2rem 1rem 4rem;
      position: relative;
      overflow-x: hidden;
    }

    /* CRT scanline overlay */
    body::before {
      content: '';
      pointer-events: none;
      position: fixed;
      inset: 0;
      background: repeating-linear-gradient(
        to bottom,
        transparent 0px,
        transparent 3px,
        rgba(0,0,0,0.18) 3px,
        rgba(0,0,0,0.18) 4px
      );
      z-index: 1000;
    }

    /* CRT vignette */
    body::after {
      content: '';
      pointer-events: none;
      position: fixed;
      inset: 0;
      background: radial-gradient(ellipse at center,
        transparent 55%,
        rgba(0,0,0,0.65) 100%
      );
      z-index: 999;
    }

    /* ── Header ───────────────────────────────────────────────────── */
    .terminal-header {
      width: 100%;
      max-width: 700px;
      text-align: center;
      margin-bottom: 2.5rem;
      animation: fadeIn .6s ease both;
    }

    .terminal-header .logo {
      font-family: var(--font-display);
      font-size: clamp(2.8rem, 8vw, 5rem);
      line-height: 1;
      text-shadow: var(--glow-strong);
      letter-spacing: 0.05em;
    }

    .terminal-header .logo span {
      color: var(--green-bright);
    }

    .terminal-header .subtitle {
      font-size: 0.8rem;
      color: var(--green-dim);
      letter-spacing: 0.2em;
      text-transform: uppercase;
      margin-top: .4rem;
    }

    /* blinking cursor after subtitle */
    .cursor {
      display: inline-block;
      width: .55em;
      height: 1em;
      background: var(--green);
      margin-left: 3px;
      vertical-align: middle;
      animation: blink 1s step-start infinite;
    }

    @keyframes blink { 50% { opacity: 0; } }

    /* ── Panel / Card ─────────────────────────────────────────────── */
    .panel {
      width: 100%;
      max-width: 700px;
      background: var(--bg-panel);
      border: 1px solid var(--green-dim);
      border-radius: 2px;
      box-shadow: var(--glow), inset 0 0 60px rgba(57,255,20,0.04);
      padding: 2rem 2rem 2.5rem;
      animation: fadeIn .8s .15s ease both;
      position: relative;
    }

    /* corner decorations */
    .panel::before, .panel::after {
      content: '+';
      position: absolute;
      font-family: var(--font-display);
      font-size: 1.2rem;
      color: var(--green-dim);
      line-height: 1;
    }
    .panel::before { top: 6px; left: 10px; }
    .panel::after  { bottom: 6px; right: 10px; }

    .panel-title {
      font-family: var(--font-display);
      font-size: 1.5rem;
      letter-spacing: .15em;
      color: var(--green-bright);
      text-shadow: var(--glow);
      margin-bottom: 1.5rem;
      border-bottom: 1px solid var(--green-dim);
      padding-bottom: .6rem;
    }

    /* ── Form elements ────────────────────────────────────────────── */
    label.field-label {
      display: block;
      font-size: .75rem;
      letter-spacing: .18em;
      text-transform: uppercase;
      color: var(--green-dim);
      margin-bottom: .5rem;
    }

    textarea {
      display: block;
      width: 100%;
      min-height: 160px;
      background: #000;
      border: 1px solid var(--green-dim);
      border-radius: 2px;
      color: var(--green);
      font-family: var(--font-display);
      font-size: 1.4rem;
      line-height: 1.4;
      padding: .75rem 1rem;
      resize: vertical;
      transition: border-color .2s, box-shadow .2s;
      caret-color: var(--green-bright);
    }

    textarea::placeholder { color: var(--green-dim); opacity: .6; }

    textarea:focus {
      outline: none;
      border-color: var(--green);
      box-shadow: var(--glow);
    }

    /* character counter */
    .char-count {
      font-size: .7rem;
      color: var(--green-dim);
      text-align: right;
      margin-top: .3rem;
      letter-spacing: .1em;
    }
    .char-count.warn { color: #ffaa00; text-shadow: 0 0 6px #ffaa0088; }

    /* ── Button ───────────────────────────────────────────────────── */
    .btn-transmit {
      display: block;
      width: 100%;
      margin-top: 1.5rem;
      padding: .85rem 1.5rem;
      background: transparent;
      border: 2px solid var(--green);
      border-radius: 2px;
      color: var(--green-bright);
      font-family: var(--font-display);
      font-size: 1.6rem;
      letter-spacing: .2em;
      text-transform: uppercase;
      cursor: pointer;
      box-shadow: var(--glow);
      transition: background .15s, color .15s, box-shadow .15s;
      position: relative;
      overflow: hidden;
    }

    .btn-transmit::before {
      content: '';
      position: absolute;
      inset: 0;
      background: var(--green);
      transform: translateX(-101%);
      transition: transform .2s ease;
    }

    .btn-transmit:hover::before  { transform: translateX(0); }
    .btn-transmit:hover          { color: #000; box-shadow: var(--glow-strong); }
    .btn-transmit span           { position: relative; z-index: 1; }

    /* ── Flash message ────────────────────────────────────────────── */
    .flash {
      width: 100%;
      max-width: 700px;
      margin-bottom: 1.5rem;
      padding: .75rem 1.25rem;
      border: 1px solid;
      font-size: .85rem;
      letter-spacing: .1em;
      animation: fadeIn .4s ease both;
    }
    .flash.ok  { border-color: var(--green);     color: var(--green-bright); }
    .flash.err { border-color: #ff4040;           color: #ff8080;            }

    /* ── Status bar ───────────────────────────────────────────────── */
    .status-bar {
      width: 100%;
      max-width: 700px;
      margin-top: 1.2rem;
      display: flex;
      gap: 2rem;
      font-size: .68rem;
      letter-spacing: .12em;
      color: var(--green-dim);
      animation: fadeIn 1s .3s ease both;
    }
    .status-bar .indicator {
      display: inline-block;
      width: 7px; height: 7px;
      border-radius: 50%;
      background: var(--green);
      box-shadow: var(--glow);
      margin-right: .4em;
      vertical-align: middle;
      animation: pulse 2.5s ease-in-out infinite;
    }
    @keyframes pulse {
      0%,100% { opacity: 1; }
      50%      { opacity: .3; }
    }

    /* ── Endpoint hint box ────────────────────────────────────────── */
    .endpoint-hint {
      width: 100%;
      max-width: 700px;
      margin-top: 1.5rem;
      padding: 1rem 1.25rem;
      border: 1px solid var(--green-dim);
      font-size: .75rem;
      color: var(--green-dim);
      line-height: 1.8;
      animation: fadeIn 1s .45s ease both;
    }
    .endpoint-hint code {
      color: var(--green);
      font-size: .85rem;
    }
    .endpoint-hint .hint-title {
      font-family: var(--font-display);
      font-size: 1rem;
      color: var(--green-bright);
      letter-spacing: .12em;
      margin-bottom: .5rem;
    }

    /* ── Animations ───────────────────────────────────────────────── */
    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(10px); }
      to   { opacity: 1; transform: translateY(0); }
    }

    /* ── Responsive tweaks ────────────────────────────────────────── */
    @media (max-width: 480px) {
      .panel { padding: 1.25rem 1rem 2rem; }
    }
  </style>
</head>
<body>

  <!-- Header -->
  <header class="terminal-header">
    <div class="logo">FUJI<span>NET</span></div>
    <div class="subtitle">ATARI DISPLAY TERMINAL v1.0 <span class="cursor"></span></div>
  </header>

  <!-- Flash message (shown only after redirect) -->
  {{if .Flash}}
  <div class="flash {{.FlashClass}}">▶ {{.Flash}}</div>
  {{end}}

  <!-- Main panel -->
  <div class="panel">
    <div class="panel-title">// TRANSMIT TO ATARI</div>

    <form action="/post" method="POST">
      <label class="field-label" for="msg">Enter something to display on the ATARI</label>
      <textarea
        id="msg"
        name="message"
        maxlength="1024"
        placeholder="READY."
        autofocus
      >{{.CurrentMessage}}</textarea>
      <div class="char-count" id="charCount">0 / 1024 CHARS</div>

      <button class="btn-transmit" type="submit">
        <span>⬡ TRANSMIT TO ATARI ⬡</span>
      </button>
    </form>
  </div>

  <!-- Status bar -->
  <div class="status-bar">
    <span><span class="indicator"></span>FUJINET CONNECTED</span>
    <span>SIO BUS: READY</span>
    <span>GET ENDPOINT: <code>/get</code></span>
  </div>

  <!-- Endpoint hint -->
  <div class="endpoint-hint">
    <div class="hint-title">// DEVICE ENDPOINTS</div>
    <div><code>POST /post</code>      — submit a message to memory</div>
    <div><code>GET  /get</code>       — retrieve current message as plain text</div>
    <div><code>GET  /timestamp</code> — Unix timestamp of the last received message</div>
  </div>

  <script>
    // Live character counter
    const ta = document.getElementById('msg');
    const cc = document.getElementById('charCount');
    const update = () => {
      const n = ta.value.length;
      cc.textContent = n + ' / 1024 CHARS';
      cc.classList.toggle('warn', n > 900);
    };
    ta.addEventListener('input', update);
    update();
  </script>
</body>
</html>`

// --- Template data ---

type pageData struct {
	Flash          string
	FlashClass     string
	CurrentMessage string
}

// --- Handlers ---

func handleIndex(w http.ResponseWriter, r *http.Request) {
	if r.URL.Path != "/" {
		http.NotFound(w, r)
		return
	}

	flash := r.URL.Query().Get("flash")
	flashClass := r.URL.Query().Get("fc")

	mu.RLock()
	msg := message
	mu.RUnlock()

	tmpl := template.Must(template.New("index").Parse(indexHTML))
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	err := tmpl.Execute(w, pageData{
		Flash:          flash,
		FlashClass:     flashClass,
		CurrentMessage: msg,
	})
	if err != nil {
		log.Printf("template error: %v", err)
	}
}

func handlePost(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method Not Allowed", http.StatusMethodNotAllowed)
		return
	}

	if err := r.ParseForm(); err != nil {
		http.Redirect(w, r, "/?flash=ERROR+PARSING+FORM&fc=err", http.StatusSeeOther)
		return
	}

	msg := r.FormValue("message")
	if len(msg) > 1024 {
		msg = msg[:1024]
	}

	mu.Lock()
	message = msg
	lastUpdated = time.Now()
	mu.Unlock()

	log.Printf("message updated: %q", msg)
	http.Redirect(w, r, "/?flash=MESSAGE+TRANSMITTED+OK&fc=ok", http.StatusSeeOther)
}

func handleGet(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "Method Not Allowed", http.StatusMethodNotAllowed)
		return
	}

	mu.RLock()
	msg := message
	mu.RUnlock()

	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	fmt.Fprint(w, msg)
}

func handleTimestamp(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "Method Not Allowed", http.StatusMethodNotAllowed)
		return
	}

	mu.RLock()
	ts := lastUpdated
	mu.RUnlock()

	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	fmt.Fprintf(w, "%d", ts.Unix())
}

// --- Main ---

func main() {
	http.HandleFunc("/", handleIndex)
	http.HandleFunc("/post", handlePost)
	http.HandleFunc("/get", handleGet)
	http.HandleFunc("/timestamp", handleTimestamp)

	addr := ":8080"
	log.Printf("FujiNet Display Terminal listening on %s", addr)
	log.Fatal(http.ListenAndServe(addr, nil))
}
