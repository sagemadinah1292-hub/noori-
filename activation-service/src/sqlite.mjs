import { DatabaseSync } from 'node:sqlite';
import { readFileSync } from 'node:fs';
export class LocalDatabase {
  constructor(path) {
    this.db = new DatabaseSync(path);
    this.db.exec('PRAGMA journal_mode=WAL; PRAGMA busy_timeout=5000;');
    this.db.exec(readFileSync(new URL('../migrations/0001_licenses.sql', import.meta.url), 'utf8'));
  }
  prepare(sql) {
    const statement = this.db.prepare(sql);
    return { bind: (...values) => ({
      first: async () => statement.get(...values) ?? null,
      run: async () => statement.run(...values)
    }) };
  }
}
