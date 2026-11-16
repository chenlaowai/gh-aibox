type EventNames = 'API:UN_AUTH' | 'API:VALIDATE_ERROR' | 'API:SYSTEM_ERROR' | 'API:TOKEN_EXPIRE' | 'ACTION:LOGOUT' | 'ACTION:TOHOME'

class EventEmitter {
  private listeners: Record<EventNames, Set<any>> = {
    'API:UN_AUTH': new Set(),
    'API:VALIDATE_ERROR': new Set(),
    'API:SYSTEM_ERROR': new Set(),
    'API:TOKEN_EXPIRE': new Set(),
    'ACTION:LOGOUT': new Set(),
    'ACTION:TOHOME': new Set(),
  }

  on(eventName: EventNames, listener: (...args: any[]) => void) {
    this.listeners[eventName].add(listener)
  }

  emit(eventName: EventNames, ...args: any[]) {
    this.listeners[eventName].forEach(listener => listener(...args))
  }
}

export const emitter = new EventEmitter()
