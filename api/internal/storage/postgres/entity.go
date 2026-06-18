package postgres

import (
	"time"

	"github.com/google/uuid"
)

type Entity interface {
	GetID() uuid.UUID
	GetCreateDate() *time.Time
	GetUpdateDate() *time.Time
}

type BaseEntity struct {
	ID         uuid.UUID  `gorm:"type:uuid;primaryKey;default:gen_random_uuid()"`
	CreateDate *time.Time `gorm:"column:created_at;autoCreateTime"`
	UpdateDate *time.Time `gorm:"column:updated_at;autoUpdateTime"`
}

func (e BaseEntity) GetID() uuid.UUID {
	return e.ID
}

func (e BaseEntity) GetCreateDate() *time.Time {
	return e.CreateDate
}

func (e BaseEntity) GetUpdateDate() *time.Time {
	return e.UpdateDate
}
