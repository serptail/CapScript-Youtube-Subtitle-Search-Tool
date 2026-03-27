import { env, createExecutionContext, waitOnExecutionContext } from 'cloudflare:test';
import { describe, it, expect } from 'vitest';
import worker from '../src/index';

const IncomingRequest = Request<unknown, IncomingRequestCfProperties>;

describe('capscript-feedback-worker', () => {
    it('rejects non-POST methods', async () => {
        const req = new IncomingRequest('https://example.com', { method: 'GET' });
        const ctx = createExecutionContext();
        const res = await worker.fetch(req, env, ctx);
        await waitOnExecutionContext(ctx);
        expect(res.status).toBe(405);
    });

    it('handles CORS preflight', async () => {
        const req = new IncomingRequest('https://example.com', { method: 'OPTIONS' });
        const ctx = createExecutionContext();
        const res = await worker.fetch(req, env, ctx);
        await waitOnExecutionContext(ctx);
        expect(res.status).toBe(204);
        expect(res.headers.get('Access-Control-Allow-Methods')).toContain('POST');
    });

    it('rejects invalid JSON', async () => {
        const req = new IncomingRequest('https://example.com', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: 'not-json',
        });
        const ctx = createExecutionContext();
        const res = await worker.fetch(req, env, ctx);
        await waitOnExecutionContext(ctx);
        expect(res.status).toBe(400);
        const body = await res.json() as any;
        expect(body.error).toContain('Invalid JSON');
    });

    it('rejects out-of-range rating', async () => {
        const req = new IncomingRequest('https://example.com', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ rating: 9, text: 'hello' }),
        });
        const ctx = createExecutionContext();
        const res = await worker.fetch(req, env, ctx);
        await waitOnExecutionContext(ctx);
        expect(res.status).toBe(400);
        const body = await res.json() as any;
        expect(body.error).toMatch(/Rating must be/);
    });
});

