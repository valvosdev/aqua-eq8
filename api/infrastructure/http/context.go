package http

import (
	"context"

	"github.com/google/uuid"
)

type contextKey string

const userMetadataContextKey contextKey = "user_metadata"

type UserMetadata struct {
	UserID uuid.UUID
	Roles  []string

	TenantID uuid.UUID //tentantID
	Extra    map[string]any
}

func ContextWithUserMetadata(ctx context.Context, user UserMetadata) context.Context {
	return context.WithValue(ctx, userMetadataContextKey, user)
}

func UserMetadataFromContext(ctx context.Context) (UserMetadata, bool) {
	meta, ok := ctx.Value(userMetadataContextKey).(UserMetadata)
	return meta, ok
}
