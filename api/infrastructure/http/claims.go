package http

import (
	"github.com/golang-jwt/jwt/v5"
	"github.com/google/uuid"
)

type JwtClaims struct {
	UserID   uuid.UUID `json:"user_id"`
	TenantID uuid.UUID `json:"tenant_id"`
	Roles    []string  `json:"roles"`
	Name     string    `json:"name"`
	jwt.RegisteredClaims
}
