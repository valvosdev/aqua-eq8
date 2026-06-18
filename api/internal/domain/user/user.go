package user

import (
	"time"

	"github.com/google/uuid"
	"github.com/valvosdev/aqua-eq8/api/internal/storage/postgres"
)

type UserStatus string

const (
	UserStatusActive   UserStatus = "ACTIVE"
	UserStatusInactive UserStatus = "INACTIVE"
	UserStatusBlocked  UserStatus = "BLOCKED"
)

type User struct {
	postgres.BaseEntity
	TenantID     uuid.UUID  `gorm:"column:company_id;type:uuid;index"`
	Email        string     `gorm:"column:email;size:255;not null;uniqueIndex"`
	PasswordHash string     `gorm:"column:password_hash;size:255;not null"`
	FirstName    string     `gorm:"column:first_name;size:100;not null"`
	LastName     string     `gorm:"column:last_name;size:100;not null"`
	Phone        string     `gorm:"column:phone;size:50"`
	AvatarURL    string     `gorm:"column:avatar_url;size:500"`
	Status       UserStatus `gorm:"column:status;size:50;not null;default:INACTIVE"`
	IsVerified   bool       `gorm:"column:is_verified;not null;default:false"`
	LastLoginAt  *time.Time `gorm:"column:last_login_at"`
	Timezone     string     `gorm:"column:timezone;size:100"`
	Language     string     `gorm:"column:language;size:10"`
	Role         string     `gorm:"column:role;size:50;not null"`
}

func (u *User) IsActive() bool {
	return u.Status == UserStatusActive
}

func (u *User) Fullname() string {
	return u.FirstName + " " + u.LastName
}
