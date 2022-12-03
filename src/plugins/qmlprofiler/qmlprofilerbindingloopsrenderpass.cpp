/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "qmlprofilerbindingloopsrenderpass.h"
#include <utils/qtcassert.h>

#include <utils/theme/theme.h>

namespace QmlProfiler {
namespace Internal {

class BindingLoopMaterial : public QSGMaterial {
public:
    QSGMaterialType *type() const override;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QSGMaterialShader *createShader() const override;
#else
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;
#endif // < Qt 6
    BindingLoopMaterial();
};

class BindingLoopsRenderPassState : public Timeline::TimelineRenderPass::State {
public:
    BindingLoopsRenderPassState(const QmlProfilerRangeModel *model);
    ~BindingLoopsRenderPassState() override;

    BindingLoopMaterial *material() { return &m_material; }
    void updateIndexes(int from, int to);

    int indexFrom() const { return m_indexFrom; }
    int indexTo() const { return m_indexTo; }

    QSGNode *expandedRow(int row) const { return m_expandedRows[row]; }
    const QVector<QSGNode *> &expandedRows() const override { return m_expandedRows; }
    QSGNode *collapsedOverlay() const override { return m_collapsedOverlay; }

private:
    QVector<QSGNode *> m_expandedRows;
    QSGNode *m_collapsedOverlay;
    BindingLoopMaterial m_material;
    int m_indexFrom;
    int m_indexTo;
};

struct Point2DWithOffset {
    float x, y; // vec4 vertexCoord
    float x2, y2; // vec2 postScaleOffset
    void set(float nx, float ny, float nx2, float ny2);
};

struct BindlingLoopsGeometry {
    static const QSGGeometry::AttributeSet &point2DWithOffset();
    static const int maxEventsPerNode = 0xffff / 18;

    uint allocatedVertices = 0;
    uint usedVertices = 0;
    float currentY = -1;

    QSGGeometryNode *node = nullptr;
    Point2DWithOffset *vertexData();

    void allocate(QSGMaterial *material);
    void addExpandedEvent(float itemCenter);
    void addCollapsedEvent(float horizontalCenterSource, float horizontalCenterTarget,
                           float verticalCenterSource, float verticalCenterTarget);
};

const QmlProfilerBindingLoopsRenderPass *QmlProfilerBindingLoopsRenderPass::instance()
{
    static const QmlProfilerBindingLoopsRenderPass pass;
    return &pass;
}

QmlProfilerBindingLoopsRenderPass::QmlProfilerBindingLoopsRenderPass() = default;

static inline bool eventOutsideRange(const QmlProfilerRangeModel *model,
                                  const Timeline::TimelineRenderState *parentState, int i)
{
    const qint64 start = qMax(parentState->start(), model->startTime(i));
    const qint64 end = qMin(parentState->end(), model->endTime(i));
    return start > end;
}

void updateNodes(const QmlProfilerRangeModel *model, int from, int to,
                 const Timeline::TimelineRenderState *parentState,
                 BindingLoopsRenderPassState *state)
{
    QVector<BindlingLoopsGeometry> expandedPerRow(model->expandedRowCount());
    BindlingLoopsGeometry collapsed;

    for (int i = from; i < to; ++i) {
        int bindingLoopDest = model->bindingLoopDest(i);
        if (bindingLoopDest == -1 || eventOutsideRange(model, parentState, i))
            continue;

        expandedPerRow[model->expandedRow(i)].usedVertices += 4;
        collapsed.usedVertices += 18;
    }

    for (int i = 0; i < model->expandedRowCount(); ++i) {
        BindlingLoopsGeometry &row = expandedPerRow[i];
        if (row.usedVertices > 0) {
            row.allocate(state->material());
            state->expandedRow(i)->appendChildNode(row.node);
        }
    }

    if (collapsed.usedVertices > 0) {
        collapsed.allocate(state->material());
        state->collapsedOverlay()->appendChildNode(collapsed.node);
    }

    int rowHeight = Timeline::TimelineModel::defaultRowHeight();
    for (int i = from; i < to; ++i) {
        int bindingLoopDest = model->bindingLoopDest(i);
        if (bindingLoopDest == -1 || eventOutsideRange(model, parentState, i))
            continue;

        qint64 center = qMax(parentState->start(), qMin(parentState->end(),
                                                        (model->startTime(i) + model->endTime(i)) /
                                                        (qint64)2));

        float itemCenter = (center - parentState->start()) * parentState->scale();
        expandedPerRow[model->expandedRow(i)].addExpandedEvent(itemCenter);

        center = qMax(parentState->start(), qMin(parentState->end(),
                                                 (model->startTime(bindingLoopDest) +
                                                  model->endTime(bindingLoopDest)) / (qint64)2));

        float itemCenterTarget = (center - parentState->start()) * parentState->scale();
        collapsed.addCollapsedEvent(itemCenter, itemCenterTarget,
                                    (model->collapsedRow(i) + 0.5) * rowHeight,
                                    (model->collapsedRow(bindingLoopDest) + 0.5) * rowHeight);
    }
}

Timeline::TimelineRenderPass::State *QmlProfilerBindingLoopsRenderPass::update(
        const Timeline::TimelineAbstractRenderer *renderer,
        const Timeline::TimelineRenderState *parentState, State *oldState,
        int indexFrom, int indexTo, bool stateChanged, float spacing) const
{
    Q_UNUSED(stateChanged)
    Q_UNUSED(spacing)

    auto model = qobject_cast<const QmlProfilerRangeModel *>(renderer->model());

    if (!model || indexFrom < 0 || indexTo > model->count() || indexFrom >= indexTo)
        return oldState;

    BindingLoopsRenderPassState *state;
    if (oldState == nullptr)
        state = new BindingLoopsRenderPassState(model);
    else
        state = static_cast<BindingLoopsRenderPassState *>(oldState);

    if (state->indexFrom() < state->indexTo()) {
        if (indexFrom < state->indexFrom()) {
            for (int i = indexFrom; i < state->indexFrom();
                 i += BindlingLoopsGeometry::maxEventsPerNode)
                updateNodes(model, i, qMin(i + BindlingLoopsGeometry::maxEventsPerNode,
                                           state->indexFrom()), parentState, state);
        }
        if (indexTo > state->indexTo()) {
            for (int i = state->indexTo(); i < indexTo; i+= BindlingLoopsGeometry::maxEventsPerNode)
                updateNodes(model, i, qMin(i + BindlingLoopsGeometry::maxEventsPerNode, indexTo),
                            parentState, state);
        }
    } else {
        for (int i = indexFrom; i < indexTo; i+= BindlingLoopsGeometry::maxEventsPerNode)
            updateNodes(model, i, qMin(i + BindlingLoopsGeometry::maxEventsPerNode, indexTo),
                        parentState, state);
    }

    state->updateIndexes(indexFrom, indexTo);
    return state;
}

const QSGGeometry::AttributeSet &BindlingLoopsGeometry::point2DWithOffset()
{
    static const QSGGeometry::Attribute data[] = {
        // vec4 vertexCoord
        QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType,
                                                        QSGGeometry::PositionAttribute),
        // vec2 postScaleOffset
        QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType,
                                                        QSGGeometry::UnknownAttribute),
    };
    static const QSGGeometry::AttributeSet attrs = {
        sizeof(data) / sizeof(data[0]),
        sizeof(Point2DWithOffset),
        data
    };
    return attrs;
}

Point2DWithOffset *BindlingLoopsGeometry::vertexData()
{
    QSGGeometry *geometry = node->geometry();
    Q_ASSERT(geometry->attributeCount() == 2);
    Q_ASSERT(geometry->sizeOfVertex() == sizeof(Point2DWithOffset));
    const QSGGeometry::Attribute *attributes = geometry->attributes();
    Q_ASSERT(attributes[0].position == 0);
    Q_ASSERT(attributes[0].tupleSize == 2);
    Q_ASSERT(attributes[0].type == QSGGeometry::FloatType);
    Q_ASSERT(attributes[1].position == 1);
    Q_ASSERT(attributes[1].tupleSize == 2);
    Q_ASSERT(attributes[1].type == QSGGeometry::FloatType);
    Q_UNUSED(attributes)
    return static_cast<Point2DWithOffset *>(geometry->vertexData());
}

void BindlingLoopsGeometry::allocate(QSGMaterial *material)
{
    auto geometry = new QSGGeometry(BindlingLoopsGeometry::point2DWithOffset(), usedVertices);
    Q_ASSERT(geometry->vertexData());
    geometry->setIndexDataPattern(QSGGeometry::StaticPattern);
    geometry->setVertexDataPattern(QSGGeometry::StaticPattern);
    node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry, true);
    node->setMaterial(material);
    allocatedVertices = usedVertices;
    usedVertices = 0;
}

void BindlingLoopsGeometry::addExpandedEvent(float itemCenter)
{
    float verticalCenter = Timeline::TimelineModel::defaultRowHeight() / 2.0f;
    Point2DWithOffset *v = vertexData() + usedVertices;
    v[0].set(itemCenter, verticalCenter, -1.0f, currentY);
    v[1].set(itemCenter, verticalCenter, +1.0f, currentY);
    currentY = -currentY;
    v[2].set(itemCenter, verticalCenter, -1.0f, currentY);
    v[3].set(itemCenter, verticalCenter, +1.0f, currentY);
    usedVertices += 4;
}

void BindlingLoopsGeometry::addCollapsedEvent(float horizontalCenterSource,
                                              float horizontalCenterTarget,
                                              float verticalCenterSource,
                                              float verticalCenterTarget)
{
    // The source event should always be above the parent event because ranges are perfectly nested
    // and events are ordered by start time.
    QTC_ASSERT(verticalCenterSource > verticalCenterTarget, /**/);

    float tilt = horizontalCenterSource < horizontalCenterTarget ? +0.3f : -0.3f;

    Point2DWithOffset *v = vertexData() + usedVertices;
    v[0].set(horizontalCenterSource, verticalCenterSource, -0.3f, tilt);
    v[1].set(horizontalCenterSource, verticalCenterSource, -0.3f, tilt);
    v[2].set(horizontalCenterSource, verticalCenterSource, +0.3f, -tilt);

    v[3].set(horizontalCenterTarget, verticalCenterTarget, -0.3f, tilt);
    v[4].set(horizontalCenterTarget, verticalCenterTarget, +0.3f, -tilt);
    v[5].set(horizontalCenterTarget, verticalCenterTarget, -1.0f, -1.0f);
    v[6].set(horizontalCenterTarget, verticalCenterTarget, +1.0f, -1.0f);
    v[7].set(horizontalCenterTarget, verticalCenterTarget, -1.0f, +1.0f);
    v[8].set(horizontalCenterTarget, verticalCenterTarget, +1.0f, +1.0f);
    v[9].set(horizontalCenterTarget, verticalCenterTarget, -0.3f, tilt);
    v[10].set(horizontalCenterTarget, verticalCenterTarget, +0.3f, -tilt);

    v[11].set(horizontalCenterSource, verticalCenterSource, -0.3f, tilt);
    v[12].set(horizontalCenterSource, verticalCenterSource, +0.3f, -tilt);
    v[13].set(horizontalCenterSource, verticalCenterSource, -1.0f, +1.0f);
    v[14].set(horizontalCenterSource, verticalCenterSource, +1.0f, +1.0f);
    v[15].set(horizontalCenterSource, verticalCenterSource, -1.0f, -1.0f);
    v[16].set(horizontalCenterSource, verticalCenterSource, +1.0f, -1.0f);
    v[17].set(horizontalCenterSource, verticalCenterSource, +1.0f, -1.0f);

    usedVertices += 18;
}

class BindingLoopMaterialShader : public QSGMaterialShader
{
public:
    BindingLoopMaterialShader();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void updateState(const RenderState &state, QSGMaterial *newEffect,
                     QSGMaterial *oldEffect) override;
    char const *const *attributeNames() const override;
#else // < Qt 6
    bool updateUniformData(RenderState &state, QSGMaterial *, QSGMaterial *) override;
#endif // < Qt 6

private:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void initialize() override;

    int m_matrix_id = 0;
    int m_z_range_id = 0;
    int m_color_id = 0;
#endif // < Qt 6
};

BindingLoopMaterialShader::BindingLoopMaterialShader()
    : QSGMaterialShader()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    setShaderSourceFile(QOpenGLShader::Vertex,
                        QStringLiteral(":/QtCreator/QmlProfiler/bindingloops.vert"));
    setShaderSourceFile(QOpenGLShader::Fragment,
                        QStringLiteral(":/QtCreator/QmlProfiler/bindingloops.frag"));
#else // < Qt 6
    setShaderFileName(VertexStage, ":/QtCreator/QmlProfiler/bindingloops_qt6.vert.qsb");
    setShaderFileName(FragmentStage, ":/QtCreator/QmlProfiler/bindingloops_qt6.frag.qsb");
#endif // < Qt 6
}

static QColor bindingLoopsColor()
{
    return Utils::creatorTheme()->color(Utils::Theme::Timeline_HighlightColor);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void BindingLoopMaterialShader::updateState(const RenderState &state, QSGMaterial *, QSGMaterial *)
{
    if (state.isMatrixDirty()) {
        program()->setUniformValue(m_matrix_id, state.combinedMatrix());
        program()->setUniformValue(m_z_range_id, GLfloat(1.0));
        program()->setUniformValue(m_color_id, bindingLoopsColor());
    }
}
#else // < Qt 6
bool BindingLoopMaterialShader::updateUniformData(RenderState &state, QSGMaterial *, QSGMaterial *)
{
    QByteArray *buf = state.uniformData();

    // mat4 matrix
    if (state.isMatrixDirty()) {
        const QMatrix4x4 m = state.combinedMatrix();
        memcpy(buf->data(), m.constData(), 64);
    }

    // vec4 bindingLoopsColor
    const QColor color = bindingLoopsColor();
    const float colorArray[] = { color.redF(), color.greenF(), color.blueF(), color.alphaF() };
    memcpy(buf->data() + 64, colorArray, 16);

    return true;
}
#endif // < Qt 6

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
char const *const *BindingLoopMaterialShader::attributeNames() const
{
    static const char *const attr[] = {"vertexCoord", "postScaleOffset", nullptr};
    return attr;
}

void BindingLoopMaterialShader::initialize()
{
    m_matrix_id = program()->uniformLocation("matrix");
    m_z_range_id = program()->uniformLocation("_qt_zRange");
    m_color_id = program()->uniformLocation("bindingLoopsColor");
}
#endif // < Qt 6

BindingLoopMaterial::BindingLoopMaterial()
{
    setFlag(QSGMaterial::Blending, false);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    setFlag(QSGMaterial::CustomCompileStep, true);
#endif // >= Qt 6
}

QSGMaterialType *BindingLoopMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QSGMaterialShader *BindingLoopMaterial::createShader() const
#else // < Qt 6
QSGMaterialShader *BindingLoopMaterial::createShader(QSGRendererInterface::RenderMode) const
#endif // < Qt 6
{
    return new BindingLoopMaterialShader;
}

void Point2DWithOffset::set(float nx, float ny, float nx2, float ny2)
{
    x = nx; y = ny; x2 = nx2; y2 = ny2;
}

BindingLoopsRenderPassState::BindingLoopsRenderPassState(const QmlProfilerRangeModel *model) :
    m_indexFrom(std::numeric_limits<int>::max()), m_indexTo(-1)
{
    m_collapsedOverlay = new QSGNode;
    m_collapsedOverlay->setFlag(QSGNode::OwnedByParent, false);
    m_expandedRows.reserve(model->expandedRowCount());
    for (int i = 0; i < model->expandedRowCount(); ++i) {
        auto node = new QSGNode;
        node->setFlag(QSGNode::OwnedByParent, false);
        m_expandedRows << node;
    }
}

BindingLoopsRenderPassState::~BindingLoopsRenderPassState()
{
    delete m_collapsedOverlay;
    qDeleteAll(m_expandedRows);
}

void BindingLoopsRenderPassState::updateIndexes(int from, int to)
{
    if (from < m_indexFrom)
        m_indexFrom = from;
    if (to > m_indexTo)
        m_indexTo = to;
}

} // namespace Internal
} // namespace QmlProfiler
