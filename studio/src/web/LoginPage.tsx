import { type FormEvent, useState } from "react";
import { login } from "./api";

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
    <form className="card" onSubmit={submit}>
      <h2>{name === null ? "Sign in" : `Sign in to ${name}`}</h2>
      {note !== undefined && <p className="muted">{note}</p>}
      <div className="form-grid column">
        <label>
          Password
          <input type="password" autoComplete="current-password" autoFocus value={password} onChange={(e) => setPassword(e.target.value)} />
        </label>
      </div>
      {error !== null && <div className="banner error">{error}</div>}
      <div className="row end">
        <button type="submit" className="primary" disabled={busy || password === ""}>
          Sign in
        </button>
      </div>
    </form>
  );
}

export function LoginPage({ name, onSignedIn }: Props) {
  return (
    <div className="login">
      <LoginForm name={name} onSignedIn={onSignedIn} />
    </div>
  );
}
