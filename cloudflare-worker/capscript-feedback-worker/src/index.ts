export interface Env {
    RESEND_API_KEY: string;
    RATE_LIMITER: KVNamespace;
}

const CORS_HEADERS = {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "POST, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type",
};

function jsonResponse(body: unknown, status: number): Response {
    return new Response(JSON.stringify(body), {
        status,
        headers: { "Content-Type": "application/json", ...CORS_HEADERS },
    });
}

export default {
    async fetch(
        request: Request,
        env: Env,
        ctx: ExecutionContext
    ): Promise<Response> {
        if (request.method === "OPTIONS") {
            return new Response(null, { status: 204, headers: CORS_HEADERS });
        }
        if (request.method !== "POST") {
            return jsonResponse({ error: "Method not allowed" }, 405);
        }

        const ip = request.headers.get("CF-Connecting-IP") || "unknown";
        const limitKey = `ratelimit_${ip}`;

        const currentCount = await env.RATE_LIMITER.get(limitKey);

        if (currentCount && parseInt(currentCount) >= 10) {
            return jsonResponse(
                { error: "Too many requests. Please try again later." },
                429
            );
        }

        let body: any;
        try {
            body = await request.json();
        } catch {
            return jsonResponse({ error: "Invalid JSON" }, 400);
        }

        const rating = typeof body.rating === "number" ? body.rating : 0;
        const text = (body.text || "").toString().trim();
        const email = (body.email || "").toString().trim();

        if (rating < 0 || rating > 5)
            return jsonResponse({ error: "Invalid rating" }, 400);
        if (text.length > 2000)
            return jsonResponse({ error: "Text too long" }, 400);

        const escapeHtml = (value = "") =>
            String(value)
                .replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;")
                .replace(/"/g, "&quot;")
                .replace(/'/g, "&#39;");

        const safeHtml = `
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <meta http-equiv="X-UA-Compatible" content="IE=edge" />
  <title>Feedback Received</title>
</head>
<body style="margin:0; padding:0; background:#f8fafc; font-family:-apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; color:#0f172a;">
  <table role="presentation" width="100%" cellspacing="0" cellpadding="0" border="0" style="background:#f8fafc; width:100%; padding:32px 16px;">
    <tr>
      <td align="center">
        <table role="presentation" width="100%" cellspacing="0" cellpadding="0" border="0" style="max-width:640px; background:#ffffff; border:1px solid #e2e8f0; border-radius:14px; overflow:hidden;">
          
          <tr>
            <td style="padding:28px 32px 20px 32px; border-bottom:1px solid #e2e8f0;">
              <div style="font-size:12px; font-weight:700; letter-spacing:0.08em; text-transform:uppercase; color:#64748b; margin-bottom:8px;">
                CapScript Pro
              </div>
              <div style="font-size:22px; line-height:1.3; font-weight:700; color:#0f172a; margin:0;">
                New feedback received
              </div>
              <div style="font-size:14px; line-height:1.6; color:#475569; margin-top:8px;">
                A user has submitted feedback through the desktop client.
              </div>
            </td>
          </tr>

          <tr>
            <td style="padding:24px 32px 8px 32px;">
              <table role="presentation" width="100%" cellspacing="0" cellpadding="0" border="0" style="border:1px solid #e2e8f0; border-radius:12px;">
                <tr>
                  <td style="padding:18px 20px; border-bottom:1px solid #e2e8f0;">
                    <div style="font-size:12px; font-weight:700; letter-spacing:0.08em; text-transform:uppercase; color:#64748b; margin-bottom:8px;">
                      Rating
                    </div>
                    <div style="font-size:30px; line-height:1; font-weight:700; color:#0f172a;">
                      ${Number(rating)}<span style="font-size:16px; font-weight:600; color:#64748b;"> / 5</span>
                    </div>
                    <div style="margin-top:8px; font-size:14px; color:#475569;">
                      ${"★".repeat(Math.max(0, Math.min(5, Number(rating))))}${"☆".repeat(5 - Math.max(0, Math.min(5, Number(rating))))}
                    </div>
                  </td>
                </tr>
                <tr>
                  <td style="padding:18px 20px;">
                    <table role="presentation" width="100%" cellspacing="0" cellpadding="0" border="0">
                      <tr>
                        <td style="padding:0 0 12px 0;">
                          <div style="font-size:12px; font-weight:700; letter-spacing:0.08em; text-transform:uppercase; color:#64748b; margin-bottom:4px;">
                            From
                          </div>
                          <div style="font-size:15px; line-height:1.5; color:#0f172a; word-break:break-word;">
                            ${escapeHtml(email)}
                          </div>
                        </td>
                      </tr>
                      <tr>
                        <td>
                          <div style="font-size:12px; font-weight:700; letter-spacing:0.08em; text-transform:uppercase; color:#64748b; margin-bottom:4px;">
                            Date
                          </div>
                          <div style="font-size:15px; line-height:1.5; color:#0f172a;">
                            ${new Intl.DateTimeFormat("en-US", {
            year: "numeric",
            month: "short",
            day: "2-digit",
            hour: "2-digit",
            minute: "2-digit"
        }).format(new Date())}
                          </div>
                        </td>
                      </tr>
                    </table>
                  </td>
                </tr>
              </table>
            </td>
          </tr>

          <tr>
            <td style="padding:16px 32px 8px 32px;">
              <div style="font-size:12px; font-weight:700; letter-spacing:0.08em; text-transform:uppercase; color:#64748b; margin-bottom:10px;">
                Message
              </div>
              <div style="background:#f8fafc; border:1px solid #e2e8f0; border-radius:12px; padding:20px; font-size:15px; line-height:1.7; color:#0f172a; white-space:pre-wrap; word-break:break-word;">
                ${escapeHtml(text)}
              </div>
            </td>
          </tr>

          <tr>
            <td style="padding:24px 32px 28px 32px; color:#64748b; font-size:12px; line-height:1.6; border-top:1px solid #e2e8f0;">
              Sent from <strong style="color:#334155;">CapScript Pro</strong> Desktop Client.
              <br />
              This message was generated automatically.
            </td>
          </tr>

        </table>
      </td>
    </tr>
  </table>
</body>
</html>
`;

        try {
            const resendReq = await fetch("https://api.resend.com/emails", {
                method: "POST",
                headers: {
                    "Content-Type": "application/json",
                    Authorization: `Bearer ${env.RESEND_API_KEY}`,
                },
                body: JSON.stringify({
                    from: "CapScript Pro <onboarding@resend.dev>",
                    to: ["delivered@resend.dev"],
                    subject: `CapScript Pro Feedback (${rating}/5)`,
                    html: safeHtml,
                }),
            });

            if (!resendReq.ok) {
                console.error("Resend Error:", await resendReq.text());
                return jsonResponse({ error: "Failed to send email" }, 500);
            }

            await env.RATE_LIMITER.put(
                limitKey,
                String(parseInt(currentCount || "0") + 1),
                {
                    expirationTtl: 60,
                }
            );

            return jsonResponse({ success: true }, 200);
        } catch (error: any) {
            return jsonResponse({ error: error.message }, 500);
        }
    },
};
