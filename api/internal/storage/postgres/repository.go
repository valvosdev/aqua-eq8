package postgres

import (
	"context"

	"github.com/google/uuid"
)

type Repository[T Entity] interface {
	Insert(ctx context.Context, entity *T) (*T, error)
	Update(ctx context.Context, entity *T) (*T, error)
	Delete(ctx context.Context, entity *T) error

	FindOne(ctx context.Context, filters map[string]any, preloads ...string) (*T, error)

	GetById(ctx context.Context, id uuid.UUID, preloads ...string) (*T, error)
	GetAll(ctx context.Context) ([]T, error)

	GetFiltered(ctx context.Context, opts QueryOptions) (*PaginatedResult[T], error)
}
