import { type FormEvent, useState } from "react";
import { login } from "./api";
import { PillButton } from "./ui/controls";
import { Banner, Panel } from "./ui/layout";

interface Props {
  name: string | null;
  note?: string;
  onSignedIn: () => void;
}

export function LoginForm({ name, note, onSignedIn }: Props) {
  const [password, setPassword] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  async function submit(event: FormEvent) {
    event.preventDefault();
    setBusy(true);
    setError(null);
    try {
      await login(password);
      onSignedIn();
    } catch (err) {
      setError((err as Error).message);
      setBusy(false);
    }
  }

  return (
    <form className="ui-login-form" onSubmit={submit}>
      <Panel title={name === null ? "Sign in" : `Sign in to ${name}`}>
        <div className="ui-stack">
          {note !== undefined && <p className="ui-muted">{note}</p>}
          <label className="ui-field full">
            <span className="ui-label">Password</span>
            <input className="ui-input full" type="password" autoComplete="current-password" autoFocus value={password} onChange={(e) => setPassword(e.target.value)} />
          </label>
          {error !== null && <Banner kind="error">{error}</Banner>}
          <div className="ui-line end">
            <PillButton kind="primary" type="submit" disabled={busy || password === ""}>
              Sign in
            </PillButton>
          </div>
        </div>
      </Panel>
    </form>
  );
}

export function LoginPage({ name, onSignedIn }: Props) {
  return (
    <div className="ui-login">
      <h1 className="ui-login-brand">cable-twii Studio</h1>
      <LoginForm name={name} onSignedIn={onSignedIn} />
    </div>
  );
}
