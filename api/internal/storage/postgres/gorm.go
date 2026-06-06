package postgres

import (
	"context"
	"errors"
	"fmt"

	"math"

	"github.com/google/uuid"
	"gorm.io/gorm"
)

type GormRepository[T Entity] struct {
	db *gorm.DB
}

func NewRepository[T Entity](db *gorm.DB) Repository[T] {
	return &GormRepository[T]{
		db: db,
	}
}

// Insert creates a new record. GORM populates the ID and create_date into the pointer.
func (r *GormRepository[T]) Insert(ctx context.Context, entity *T) (*T, error) {
	err := r.db.WithContext(ctx).
		Session(&gorm.Session{FullSaveAssociations: true}).
		Create(entity).Error

	if err != nil {
		return nil, fmt.Errorf("repository insert failed: %w", err)
	}
	return entity, nil
}

// Update saves all fields of the given entity and updates the update_date column.
func (r *GormRepository[T]) Update(ctx context.Context, entity *T) (*T, error) {
	// FullSaveAssociations tells GORM to look into child slices
	// and execute INSERT queries for newly added elements automatically.
	err := r.db.WithContext(ctx).
		Session(&gorm.Session{FullSaveAssociations: true}).
		Save(entity).Error

	if err != nil {
		return nil, fmt.Errorf("repository full update failed: %w", err)
	}
	return entity, nil
}

// Delete removes a record by its primary key.
func (r *GormRepository[T]) Delete(ctx context.Context, entity *T) error {
	if err := r.db.WithContext(ctx).Delete(entity).Error; err != nil {
		return fmt.Errorf("failed to delete entity: %w", err)
	}
	return nil
}

// GetById fetches a single record using its primary key ID string.
func (r *GormRepository[T]) GetById(ctx context.Context, id uuid.UUID, preloads ...string) (*T, error) {
	var dest T

	query := r.db.WithContext(ctx)

	for _, preload := range preloads {
		query = query.Preload(preload)
	}

	err := query.First(&dest, "id = ?", id).Error
	if err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			return nil, fmt.Errorf("entity with id %s not found", id)
		}
		return nil, fmt.Errorf("failed to fetch entity: %w", err)
	}
	return &dest, nil
}

func (r *GormRepository[T]) FindOne(ctx context.Context, filters map[string]any, preloads ...string) (*T, error) {
	var dest T

	query := r.db.WithContext(ctx)

	for _, preload := range preloads {
		query = query.Preload(preload)
	}

	err := query.Where(filters).First(&dest).Error
	if err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			return nil, fmt.Errorf("entity with id %s not found", filters)
		}
		return nil, fmt.Errorf("failed to fetch entity: %w", err)
	}
	return &dest, nil
}

// GetAll fetches all records corresponding to the entity table.
func (r *GormRepository[T]) GetAll(ctx context.Context) ([]T, error) {
	var results []T
	if err := r.db.WithContext(ctx).Find(&results).Error; err != nil {
		return nil, fmt.Errorf("failed to fetch all entities: %w", err)
	}
	return results, nil
}

// GetFiltered fetches records adhering to limits, offsets, and dynamic criteria filters.
func (r *GormRepository[T]) GetFiltered(ctx context.Context, opts QueryOptions) (*PaginatedResult[T], error) {
	var results []T
	var totalCount int64

	// Initialize the foundational model query block
	query := r.db.WithContext(ctx).Model(new(T))

	// 1. Build and apply dynamic filtering rules
	for _, f := range opts.Filters {
		clause := fmt.Sprintf("%s %s ?", f.Field, f.Operator)
		query = query.Where(clause, f.Value)
	}

	// 2. Count the total matching dataset records before pagination cuts the window
	if err := query.Count(&totalCount).Error; err != nil {
		return nil, fmt.Errorf("failed to calculate dataset total: %w", err)
	}

	// 3. Fallback normalization check for basic query paging parameters
	if opts.Limit <= 0 {
		opts.Limit = 10 // Safe fallback limit execution
	}
	if opts.Page <= 0 {
		opts.Page = 1 // Safe page alignment
	}

	// 4. Calculate database offset indexing parameters
	offset := (opts.Page - 1) * opts.Limit
	totalPages := int(math.Ceil(float64(totalCount) / float64(opts.Limit)))

	// 5. Execute paginated select fetch operations
	if err := query.Limit(opts.Limit).Offset(offset).Find(&results).Error; err != nil {
		return nil, fmt.Errorf("failed to execute paginated fetch: %w", err)
	}

	return &PaginatedResult[T]{
		Data:       results,
		TotalCount: totalCount,
		TotalPages: totalPages,
		Page:       opts.Page,
		Limit:      opts.Limit,
	}, nil
}
