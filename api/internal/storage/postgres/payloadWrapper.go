package postgres

import (
	"database/sql/driver"
	"encoding/json"
	"fmt"
)

type JsonPayloadWrapper[T any] struct {
	Data T
}

func (p JsonPayloadWrapper[T]) Value() (driver.Value, error) {
	return json.Marshal(p.Data)
}

func (p *JsonPayloadWrapper[T]) Scan(value any) error {
	bytes, ok := value.([]byte)
	if !ok {
		return fmt.Errorf("failed to unmarshal JSONB payload: invalid type assertion")
	}
	return json.Unmarshal(bytes, &p.Data)
}
