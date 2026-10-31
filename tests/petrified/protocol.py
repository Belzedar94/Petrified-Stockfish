"""Bounded UCI transport used by differential tests and the independent referee."""
import queue
import subprocess
import threading
import time


class Uci:
    def __init__(self, executable, *, network=None, timeout=30, initialize=True, log_prefix=None):
        self.timeout = timeout
        self.lines = queue.Queue()
        argv=[str(x) for x in executable] if isinstance(executable,(list,tuple)) else [str(executable)]
        self.logs=[]
        if log_prefix:
            self.logs=[open(str(log_prefix)+suffix,'w',encoding='utf-8')
                       for suffix in ('.stdout.log','.stderr.log','.stdin.log')]
        self.proc = subprocess.Popen(argv, stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                     text=True, encoding='utf-8', errors='replace', bufsize=1)
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.error_reader = threading.Thread(target=self._read_error, daemon=True)
        self.reader.start()
        self.error_reader.start()
        if not initialize: return
        try:
            self.send('uci')
            self.until(lambda s: s == 'uciok')
            self.send('setoption name UCI_Variant value petrified')
            self.send('setoption name Threads value 1')
            self.send('setoption name Hash value 16')
            if network:
                self.send('setoption name EvalFile value ' + str(network))
            self.ready()
        except BaseException:
            self.close()
            raise

    def _read(self):
        for line in self.proc.stdout:
            if self.logs: self.logs[0].write(line); self.logs[0].flush()
            self.lines.put(line.rstrip())
        self.lines.put(None)

    def _read_error(self):
        for line in self.proc.stderr:
            if self.logs: self.logs[1].write(line); self.logs[1].flush()

    def send(self, command):
        if self.logs: self.logs[2].write(command+'\n'); self.logs[2].flush()
        self.proc.stdin.write(command + '\n')
        self.proc.stdin.flush()

    def until(self, predicate):
        deadline = time.monotonic() + self.timeout
        lines = []
        while True:
            try:
                line = self.lines.get(timeout=max(0, deadline-time.monotonic()))
            except queue.Empty:
                raise TimeoutError(f'PID {self.proc.pid}: timeout; last output={lines[-8:]}')
            if line is None:
                raise RuntimeError(f'PID {self.proc.pid}: exited {self.proc.poll()}; output={lines[-12:]}')
            lines.append(line)
            if predicate(line):
                return lines

    def ready(self):
        self.send('isready')
        return self.until(lambda s: s == 'readyok')

    def position(self, fen, moves=()):
        self.send('position fen ' + fen + (' moves ' + ' '.join(moves) if moves else ''))

    def fen(self):
        self.send('d')
        lines = self.ready()
        return next(s[5:] for s in lines if s.startswith('Fen: '))

    def perft(self, depth=1):
        self.send(f'go perft {depth}')
        lines = self.until(lambda s: s.startswith('Nodes searched:'))
        moves = {}
        for line in lines:
            if ': ' in line:
                move, count = line.split(': ', 1)
                if len(move) in (4, 5) and move[0] in 'abcdefgh' and count.isdigit():
                    moves[move] = int(count)
        return moves, int(lines[-1].split(':')[1])

    def close(self):
        if self.proc.poll() is None:
            try:
                self.send('quit')
                self.proc.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):
                self.proc.kill()
                self.proc.wait(timeout=5)
        self.reader.join(timeout=2)
        self.error_reader.join(timeout=2)
        for log in self.logs: log.close()
        for stream in (self.proc.stdin,self.proc.stdout,self.proc.stderr): stream.close()

    def __enter__(self): return self
    def __exit__(self, *_): self.close()
