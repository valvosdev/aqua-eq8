package authentication

import "github.com/google/uuid"

type LoginResponse struct {
	UserID   uuid.UUID `json:"user_id"`
	TenantID uuid.UUID `json:"tenant_id"`
	Roles    []string  `json:"roles"`
	Status   string    `json:"status"`
	Email    string    `json:"email"`
	Fullname string    `json:"fullname"`
}
