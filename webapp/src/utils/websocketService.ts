// WebSocketService.ts

export interface WebSocketCallbacks {
    onMessage?: (event: MessageEvent) => void;
    onOpen?: (event: Event) => void;
    onClose?: (event: CloseEvent) => void;
    onError?: (event: Event) => void;
}

export interface WebSocketOptions {
    // Close the connection while the tab is hidden and reconnect once it is
    // visible again. The device only has a few TCP connections to spare, so
    // tabs in the background must not hold websockets.
    pauseWhenHidden?: boolean;
}

// Keep the connection during short tab switches.
const HIDDEN_GRACE_MS = 10000;

export default class WebSocketService {
    private url: string;
    private socket: WebSocket | null = null;

    private reconnectDelay = 200;
    private maxDelay = 3000;

    private pauseWhenHidden: boolean;
    private hiddenTimer: number | null = null;
    private paused = false;
    private closed = false;

    private heartbeatInterval: number | null = null;
    private heartbeatTimeout = 25000; // 25s ping interval

    private onMessage: (event: MessageEvent) => void;
    private onOpen: (event: Event) => void;
    private onClose: (event: CloseEvent) => void;
    private onError: (event: Event) => void;

    constructor(url: string, callbacks: WebSocketCallbacks = {}, options: WebSocketOptions = {}) {
        this.url = url;
        this.pauseWhenHidden = options.pauseWhenHidden ?? true;
        this.onMessage = callbacks.onMessage ?? (() => {});
        this.onOpen = callbacks.onOpen ?? (() => {});
        this.onClose = callbacks.onClose ?? (() => {});
        this.onError = callbacks.onError ?? (() => {});
    }

    /** Public connect entry */
    public connect(): void {
        if (this.closed) {
            return; // a retry scheduled before close()
        }

        if (this.pauseWhenHidden) {
            document.removeEventListener('visibilitychange', this.onVisibilityChange);
            document.addEventListener('visibilitychange', this.onVisibilityChange);
            if (document.hidden) {
                // e.g. opened in a background tab: connect once it is shown
                this.paused = true;
                return;
            }
        }

        const isiOS = /iPad|iPhone|iPod/.test(navigator.userAgent);

        if (isiOS) {
            // Fix for WebKit timing bug on all iOS browsers (Safari/Chrome/Vivaldi/etc.)
            setTimeout(() => this.openSocket(), 50);
        } else {
            this.openSocket();
        }
    }

    /** Internal open function */
    private openSocket(): void {
        if (this.closed || this.paused) {
            return;
        }
        if (this.socket && this.socket.readyState <= WebSocket.OPEN) {
            return; // already connecting or connected
        }

        try {
            const socket = new WebSocket(this.url);
            this.socket = socket;

            // a socket closed by pausing may report its close after the tab
            // is visible again and the next socket is already open
            const current = () => this.socket === socket;

            socket.onopen = (event: Event) => {
                if (!current()) return;
                this.reconnectDelay = 200; // reset backoff
                this.startHeartbeat();
                this.onOpen(event);
            };

            socket.onmessage = (event: MessageEvent) => {
                if (!current()) return;
                this.onMessage(event);
            };

            socket.onclose = (event: CloseEvent) => {
                if (!current()) return;
                this.stopHeartbeat();
                this.onClose(event);
                this.scheduleReconnect();
            };

            socket.onerror = (event: Event) => {
                if (!current()) return;
                this.onError(event);
                // iOS often doesn't fire real errors, but we close to trigger retry
                socket.close();
            };
        } catch {
            this.scheduleReconnect();
        }
    }

    /** Reconnect with gradual backoff */
    private scheduleReconnect(): void {
        this.stopHeartbeat();

        if (this.closed || this.paused) {
            return;
        }

        setTimeout(() => {
            this.reconnectDelay = Math.min(this.reconnectDelay * 1.4, this.maxDelay);
            this.connect();
        }, this.reconnectDelay);
    }

    private onVisibilityChange = (): void => {
        if (document.hidden) {
            if (this.hiddenTimer === null && !this.paused) {
                this.hiddenTimer = window.setTimeout(() => {
                    this.hiddenTimer = null;
                    this.paused = true;
                    this.socket?.close();
                }, HIDDEN_GRACE_MS);
            }
            return;
        }

        if (this.hiddenTimer !== null) {
            clearTimeout(this.hiddenTimer);
            this.hiddenTimer = null;
        }
        if (this.paused) {
            this.paused = false;
            this.reconnectDelay = 200;
            this.connect();
        }
    };

    /** Heartbeat keeps NAT/websocket alive */
    private startHeartbeat(): void {
        this.stopHeartbeat();

        this.heartbeatInterval = window.setInterval(() => {
            if (this.socket?.readyState === WebSocket.OPEN) {
                this.socket.send(JSON.stringify({ type: 'ping' }));
            }
        }, this.heartbeatTimeout);
    }

    private stopHeartbeat(): void {
        if (this.heartbeatInterval !== null) {
            clearInterval(this.heartbeatInterval);
            this.heartbeatInterval = null;
        }
    }

    /** Explicit close function */
    public close(): void {
        this.closed = true;
        this.stopHeartbeat();
        document.removeEventListener('visibilitychange', this.onVisibilityChange);
        if (this.hiddenTimer !== null) {
            clearTimeout(this.hiddenTimer);
            this.hiddenTimer = null;
        }

        if (this.socket) {
            this.socket.onopen = null;
            this.socket.onmessage = null;
            this.socket.onclose = null;
            this.socket.onerror = null;

            this.socket.close();
            this.socket = null;
        }
    }
}
